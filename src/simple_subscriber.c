
/**
 * @file
 * A simple program that subscribes to a topic.
 */


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>		/* sigaction */

#include <mqtt.h>      //for pthread
#include <posix_sockets.h>
//#include <openssl_sockets.h>
#include <parse.h>
#include <mosquitto.h>
//BS1 kontron on the farm BS2 kontron at home
#define BS 2
#define IOTHUBNAME "smartherd"

#if BS == 1

#define DEVICEID "MQTTDevice"
#define PWD "SharedAccessSignature sr=smartherd.azure-devices.net%2Fdevices%2FMQTTDevice&sig=uSv7tySSUPdiwh%2BzQWmXtAbIfDz8NsUblKYvKP4QQcI%3D&se=1608502732"

#elif BS == 2

#define DEVICEID "kontronSensor"
#define PWD "SharedAccessSignature sr=smartherd.azure-devices.net%2Fdevices%2FkontronSensor&sig=EYircrlIhxTuLIcxKb95k%2FMv7A8P9H%2FzSBLrlqezAp0%3D&se=1611362416"

#endif // BS1

#define INPUTID "concentrator"

#define CERTIFICATEFILE "IoTHubRootCA_Baltimore.pem"

// computed Host Username and Topic
#define USERNAME IOTHUBNAME ".azure-devices.net/" DEVICEID "/?api-version=2018-06-30"
#define PORT_SSL_STR "8883"
#define PORT_SSL 8883
#define HOST_SSL IOTHUBNAME ".azure-devices.net"
#define TOPIC_SSL "devices/" DEVICEID "/messages/events/"

#define HOST "localhost"
#define PORT 1883

#define TOPIC_IN "mqtt-kontron/lora-gatway"
#define ADDR_IN  "localhost"
#define VERSION "6.18"



/* --- PRIVATE VARIABLES (GLOBAL) ------------------------------------------- */

typedef enum MQTTErrors MQTTErrors_t;

/**
 * signal handling variables
 */
struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
int exit_sig; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
int quit_sig; /* 1 -> application terminates without shutting down the hardware */


//struct mqtt_client clientIn;
uint8_t sendbufIn[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
uint8_t recvbufIn[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */


int sockfdIn = -1;

const char* ca_file;
const char* addrIn;
const char* addrSSL;
const char* port;
const char* portSSL;
const char* topicIn;
const char* topicSSL;
pthread_t client_daemonIn;
pthread_t wachdog_daemon;
//pthread_t client_daemonSSL;

int SSLSent = 0;
int SSLConnect = 0;
int SSLDisconnect = 0;
int wachdogFlag = 0;
int mutexWait = 0;
int msgId = 1;

char* messageToPublish;
char* messageToPublishAzure;
char strAzure[1024] = "initialization";

pthread_mutex_t mutex;
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

void sig_handler(int sigio);

/**
 * @brief The function will be called whenever a PUBLISH message is received.
 */
//void publish_callback(void** unused, struct mqtt_response_publish *published);
//void subscribe_callback(void** unused, struct mqtt_response_publish *published);
//void parse_save(const char* message, size_t size);

/**
 * @brief The client's refresher. This function triggers back-end routines to
 *        handle ingress/egress traffic to the broker.
 *
 * @note All this function needs to do is call \ref __mqtt_recv and
 *       \ref __mqtt_send every so often. I've picked 100 ms meaning that
 *       client ingress/egress traffic will be handled every 100 ms.
 */
//void* subscribe_client_refresher(void* client);
void* subscribe_mosquitto_refresher(void* client);
void* wachdog_refresher(void* client);

//int makePublisherSSL(void);
/**
    mosquitto functons
*/
void msq_connect_callback(struct mosquitto* mosq, void* obj, int result);

void msq_publish_callback(struct mosquitto* mosq, void* userdata, int mid);

void msq_disconnect_callback(struct mosquitto* mosq, void* obj, int result);

void msq_message_callback(struct mosquitto* msq, void* obj, const struct mosquitto_message* message);

int mosquitto_error(int rc, const char* message)
{
	printf("Error: %s\r\n", mosquitto_strerror(rc));

	if (message != NULL)
	{
		printf("%s\r\n", message);
	}

	//mosquitto_lib_cleanup();
	return rc;
}

/**
 * @brief Safelty closes the \p sockfd and cancels the \p client_daemon before \c exit.
 */
void simple_exit(void);

int main(int argc, const char *argv[])
{


    exit_sig = 0;
    quit_sig = 0;
    addrIn = ADDR_IN;
    port = "1883";
    topicIn = TOPIC_IN;


    ca_file = CERTIFICATEFILE;  //SSL certificate
    addrSSL = HOST_SSL;
    portSSL = PORT_SSL_STR;
    topicSSL = TOPIC_SSL;
    pthread_mutex_init(&mutex, NULL);
/****************start mosquitto part *****************************/
    int rc;
    mosquitto_lib_init();
    // create the mosquito object
    printf("Mosquitto version %d\r\n", mosquitto_lib_version(NULL,NULL,NULL));
	struct mosquitto* mosqSSL = mosquitto_new(DEVICEID, true, NULL);
	struct mosquitto* mosqIn = mosquitto_new(INPUTID, true, NULL);
	// add callback functions
	mosquitto_connect_callback_set(mosqSSL, msq_connect_callback);
	mosquitto_publish_callback_set(mosqSSL, msq_publish_callback);
	mosquitto_disconnect_callback_set(mosqSSL, msq_disconnect_callback);

	// set mosquitto username, password and options
	mosquitto_username_pw_set(mosqSSL, USERNAME, PWD);
	// specify the certificate to use
	mosquitto_tls_set(mosqSSL, CERTIFICATEFILE, NULL, NULL, NULL, NULL);

    // specify the mqtt version to use
	int option = (int)(MQTT_PROTOCOL_V311);
	rc = mosquitto_opts_set(mosqSSL, MOSQ_OPT_PROTOCOL_VERSION, &option);
	if (rc != MOSQ_ERR_SUCCESS)
	{
		mosquitto_error(rc, "Error: opts_set protocol version");
		simple_exit();
	}
	else
	{
		printf("1 Setting up options OK\r\n");
	}

	// connect

	printf("2 Connecting...\r\n");

    SSLConnect = 0;
	rc = mosquitto_connect(mosqSSL, HOST_SSL, PORT_SSL, 10);
	if (rc != MOSQ_ERR_SUCCESS)
	{
		mosquitto_error(rc,NULL);
		simple_exit();
	}

    SSLDisconnect = 0;
	printf("3 Connect returned OK\r\n");


	char mosquittoMess[100];
	sprintf(mosquittoMess,"Test 6 from Kontron N%d",msgId);


	// once connected, we can publish (send) a Telemetry message
/*
	printf("4 Publishing....\r\n");

	SSLSent = 0;
	rc = mosquitto_publish(mosqSSL, &msgId, TOPIC_SSL, strlen(mosquittoMess), mosquittoMess, 1, true);
	if (rc != MOSQ_ERR_SUCCESS)
	{
		mosquitto_error(rc,NULL);
		simple_exit();
	}

	printf("5 Publish returned OK\r\n");
*/
	// according to the mosquitto doc, a call to loop is needed when dealing with network operation
	// see https://github.com/eclipse/mosquitto/blob/master/lib/mosquitto.h
	//sleep(5);
	//printf("6 Entering Mosquitto Loop...\r\n");
    //start a thread to refresh the SSL publisher mosquitto client (handle egress and ingree client traffic)
/*
    if(pthread_create(&client_daemonSSL, NULL, SSLpublish_client_refresher, mosqSSL)) {
        fprintf(stderr, "Failed to start SSLpublisher mosquitto client daemon.\n");
       simple_exit();
    }
*/
	//mosquitto_loop_forever(mosqSSL, -1, 1);
    //sleep(5);
    //printf("first sleep\r\n");
    //mosquitto_loop(mosqSSL, -1, 1);
    //sleep(1);
    //printf("second sleep\r\n");
    /*
    while(SSLConnect == 0){
        //printf("6 Entering Mosquitto Loop...\r\n");
        mosquitto_loop(mosqSSL, -1, 1);
        //sleep(2);
	}
	*/
	mosquitto_loop(mosqSSL, -1, 1);
	printf("connected \r\n");
/*
	while(SSLSent == 0){
        //printf("6 Entering Mosquitto Loop...\r\n");
        mosquitto_loop(mosqSSL, -1, 1);
        //sleep(2);
	}
*/
/*
	SSLDisconnect = 0;
    mosquitto_disconnect(mosqSSL);
    while(SSLDisconnect == 0){
        mosquitto_loop(mosqSSL, -1, 1);
    }
    printf("7 Disconnected\r\n");
*/
	//while(1);

/****************stop mosquitto part *****************************/

/********************************start mqtt c part********************************/
/*
    // open the non-blocking TCP socket (connecting to the broker)
    sockfdIn = open_nb_socket(addrIn, port);

    if(sockfdIn == -1){
        perror("Failed to open input socket: ");
        simple_exit();
    }

    mqtt_init(&clientIn, sockfdIn, sendbufIn, sizeof(sendbufIn), recvbufIn, sizeof(recvbufIn), subscribe_callback);
    mqtt_connect(&clientIn, "subscribing_client", NULL, NULL, 0, NULL, NULL, 0, 400);

    //check that we don't have any errors
    if (clientIn.error != MQTT_OK) {
        fprintf(stderr, "connect subscriber error: %s\n", mqtt_error_str(clientIn.error));
        simple_exit();
    }

    // start a thread to refresh the subscriber client (handle egress and ingree client traffic)

    if(pthread_create(&client_daemonIn, NULL, subscribe_client_refresher, &clientIn)) {
        fprintf(stderr, "Failed to start subscriber client daemon.\n");
       simple_exit();
    }

    // subscribe
    mqtt_subscribe(&clientIn, topicIn, 0);
*/
    mosquitto_message_callback_set(mosqIn, msq_message_callback);

    printf("8 Connecting localhost\r\n");
    rc = mosquitto_connect(mosqIn, HOST, PORT, 10);
	if (rc != MOSQ_ERR_SUCCESS)
	{
		mosquitto_error(rc,NULL);
		simple_exit();
	}

	printf("9 Starting subscriber daemon\r\n");
    if(pthread_create(&client_daemonIn, NULL, subscribe_mosquitto_refresher, mosqIn)) {
        fprintf(stderr, "Failed to start subscriber client daemon.\n");
       simple_exit();
    }

    if(pthread_create(&wachdog_daemon, NULL, wachdog_refresher, NULL)) {
        fprintf(stderr, "Failed to start wachdog daemon.\n");
       simple_exit();
    }

	printf("10 Subscribing \r\n");
    int subscribeID = 1;
    rc =  mosquitto_subscribe(mosqIn, &subscribeID, TOPIC_IN, 0);
    if (rc != MOSQ_ERR_SUCCESS)
	{
		mosquitto_error(rc,NULL);
		simple_exit();
	}
/*****************************stop mqtt c part**************************************/
    printf("Version %s listening for '%s' messages.\n", VERSION, topicIn);

    /* configure signal handling */
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigact.sa_handler = sig_handler;
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
    /* block */
    while ((quit_sig != 1) && (exit_sig != 1))
    {
        while ((quit_sig != 1) && (exit_sig != 1))
        {
            if(mosquitto_loop(mosqSSL, 100, 1) == MOSQ_ERR_SUCCESS)
            {
                //if((msgId > 50)&&(SSLSent == 1))simple_exit(); //restart to fix unknown bug
                pthread_mutex_lock(&mutex);
                messageToPublishAzure = parse_get_mess(strAzure,sizeof(strAzure));
                pthread_mutex_unlock(&mutex);
                //messageToPublishAzure = parse_get_mess_azure();
                if(messageToPublishAzure != NULL)  //there is a mesasage to publish
                {
                        printf("Start.\n\r");
                        msgId++;

                        wachdogFlag = 0;

                        SSLSent = 0;
                        printf("Publishing N%d message\n\r",msgId);
                        //printf("%s\n\r",messageToPublishAzure);
                        //printf("Second message\n\r");
                        printf("%s\n\r",strAzure);
                       //rc = mosquitto_publish(mosqSSL, &msgId, TOPIC_SSL, strlen(messageToPublishAzure), messageToPublishAzure, 1, true);
                        rc = mosquitto_publish(mosqSSL, &msgId, TOPIC_SSL, strlen(strAzure), strAzure, 1, true);
                        if (rc != MOSQ_ERR_SUCCESS){
                            printf("Mosquitto error %d, can't send N%d message\n\r",rc,msgId);
                            mosquitto_error(rc,NULL);
                            printf("Exit from publishing.\n\r");
                            //break;//simple_exit();
                            //exit_example(EXIT_SUCCESS, sockfdIn, sockfdOut, sockfdOut2, &client_daemonIn, NULL,NULL);
                        }
                }

            }
            else if(SSLDisconnect == 1){
                printf("Connection lost, try to reconnect in main loop.\n\r");
                rc = mosquitto_reconnect(mosqSSL);
                if (rc != MOSQ_ERR_SUCCESS){
                    mosquitto_error(rc,NULL);
                    sleep(10);
                }
                else{
                    printf("Connection restored.\n\r");
                    SSLDisconnect = 0;
                }

            }
            else simple_exit();
        }

    }

    /* disconnect */
    printf("\n%s disconnecting from %s\n", argv[0], addrIn);
    printf("\n%s disconnecting from %s\n", argv[0], addrSSL);
    sleep(1);

    /* exit */
    //exit_example(EXIT_SUCCESS, sockfdIn, sockfdOut, &client_daemonIn, &client_daemonOut);
    simple_exit();
}

/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */


void sig_handler(int sigio) {
	if (sigio == SIGQUIT) {
		quit_sig = 1;
	} else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
		exit_sig = 1;
	}
}

void exit_example(int status, int sockfdIn, pthread_t *client_daemonIn, pthread_t *client_daemonOut,pthread_t *client_daemonOut2)
{
    if (sockfdIn != -1) close(sockfdIn);
    //if (sockfdOut != -1) close(sockfdOut);
    //if (sockfdOut2 != -1) close(sockfdOut2);
    if (client_daemonIn != NULL) pthread_cancel(*client_daemonIn);
    pthread_cancel(wachdog_daemon);
    //if (client_daemonOut != NULL) pthread_cancel(*client_daemonOut);
    //if (client_daemonOut2 != NULL) pthread_cancel(*client_daemonOut2);
   // pthread_cancel(client_daemonSSL);
    mosquitto_lib_cleanup();
    pthread_mutex_destroy(&mutex);
    exit(status);
}

void simple_exit(void)
{
    exit_example(EXIT_SUCCESS, sockfdIn, &client_daemonIn, NULL,NULL);
}
/*
void publish_callback(void** unused, struct mqtt_response_publish *published)
{
     // not used in this example
}
*/
/**
void subscribe_callback(void** unused, struct mqtt_response_publish *published)
{
    //note that published->topic_name is NOT null-terminated (here we'll change it to a c-string)
    char* topic_name = (char*) malloc(published->topic_name_size + 1);
    memcpy(topic_name, published->topic_name, published->topic_name_size);
    topic_name[published->topic_name_size] = '\0';

    //save only 32 bytes message
    if(published->application_message_size == 89)parse_save((const char*) published->application_message,published->application_message_size);

    //printf("Received publish from '%s' : %s\n", topic_name, (const char*) published->application_message);

    free(topic_name);
}
*/
/**
void* subscribe_client_refresher(void* client)
{
    while(1)
    {
        mqtt_sync((struct mqtt_client*) client);
        usleep(100000U);
    }
    return NULL;
}
*/
// Callback functions
void msq_connect_callback(struct mosquitto* mosq, void* obj, int result)
{
	printf("Connect callback returned result: %s\r\n", mosquitto_strerror(result));
    SSLConnect = 1;
	if (result == MOSQ_ERR_CONN_REFUSED)
		printf("Connection refused. Please check DeviceId, IoTHub name or if your SAS Token has expired.\r\n");
}

void msq_publish_callback(struct mosquitto* mosq, void* userdata, int mid)
{
	printf("Publish to Azure OK. \r\n");

	SSLSent = 1;
	//mosquitto_disconnect(mosq);
}
void msq_disconnect_callback(struct mosquitto* mosq, void* obj, int result)
{
	if(result == 0){
        printf("Disconnected by user.\r\n");
	}
    else{
        printf("Unexpected disconnection %d.\r\n",result);
        //return;        // simple_exit();
    }
    //else simple_exit();
    SSLDisconnect = 1;
}
/*
struct mosquitto_message{
	int mid;
	char *topic;
	void *payload;
	int payloadlen;
	int qos;
	bool retain;
};
*/

void msq_message_callback(struct mosquitto* msq, void* obj, const struct mosquitto_message* message)
{
    //save only 32 bytes message
    if(message->payloadlen == 89){
        mutexWait = 1;
        pthread_mutex_lock(&mutex);
        parse_save((const char*) message->payload,message->payloadlen);
        pthread_mutex_unlock(&mutex);
        mutexWait = 0;
       // printf("%s \r\n",(const char*) message->payload);
    }
}

void* subscribe_mosquitto_refresher(void* client)
{
    while(1)
    {
        mosquitto_loop((struct mosquitto*)client, 0, 1);
        usleep(100000U);
    }
    return NULL;
}

void* wachdog_refresher(void* client)
{
    while(1)
    {
        wachdogFlag = 1;
        sleep(60*10);
        if(wachdogFlag == 1)exit_sig = 1;
    }
    return NULL;
}

