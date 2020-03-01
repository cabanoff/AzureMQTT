
/**
 * @file
 * A simple program that subscribes to a topic.
 */


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>		/* sigaction */

#include <mqtt.h>
#include <posix_sockets.h>
//#include <openssl_sockets.h>
#include <parse.h>
#include <mosquitto.h>

// CONNECTION information to complete
#define IOTHUBNAME "smartherd"
//#define IOTHUBNAME "none"

#define DEVICEID "MQTTDevice"
#define PWD "SharedAccessSignature sr=smartherd.azure-devices.net%2Fdevices%2FMQTTDevice&sig=uSv7tySSUPdiwh%2BzQWmXtAbIfDz8NsUblKYvKP4QQcI%3D&se=1608502732"

#define CERTIFICATEFILE "IoTHubRootCA_Baltimore.pem"

// computed Host Username and Topic
#define USERNAME IOTHUBNAME ".azure-devices.net/" DEVICEID "/?api-version=2018-06-30"
#define PORT_SSL_STR "8883"
#define PORT_SSL 8883
#define HOST_SSL IOTHUBNAME ".azure-devices.net"
#define TOPIC_SSL "devices/" DEVICEID "/messages/events/"


#define TOPIC_IN "mqtt-kontron/lora-gatway"
#define ADDR_IN  "localhost"
//#define ADDR_OUT2 "mqtt.thethings.io"
//#define ADDR_OUT2 "localhost"
//#define TOPIC_OUT2 "v2/things/GfPq9BoJgm73pGrynXafogS_6kzVBo2wWnmzGCKr4J0"
//#define TOPIC_OUT2 "v2/things/1ZtGJvaiCCoVbvlliX16R7tDwh1FxYnQfQgcySsam34"
//#define TOPIC_OUT2 "v2/things/LCJzGT3QL6jucKuFcuTyBbvQzYIMunWvHUK1ZKDdfuQ"
//#define TOPIC_OUT2 "smartherd/test"

//#define ADDR_OUT "localhost"
//#define ADDR_OUT "95.46.114.123"
//#define TOPIC_OUT "smartherd/gateway1"
#define VERSION "4.18"



/* --- PRIVATE VARIABLES (GLOBAL) ------------------------------------------- */

typedef enum MQTTErrors MQTTErrors_t;

/**
 * signal handling variables
 */
struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
int exit_sig; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
int quit_sig; /* 1 -> application terminates without shutting down the hardware */


struct mqtt_client clientIn, clientOut, clientOut2;
uint8_t sendbufIn[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
uint8_t recvbufIn[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */
//uint8_t sendbufOut[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
//uint8_t recvbufOut[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */
//uint8_t sendbufOut2[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
//uint8_t recvbufOut2[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */
uint8_t sendbufSSL[2048];
uint8_t recvbufSSL[1024];


int sockfdIn = -1;
//int sockfdOut = -1;
//int sockfdOut2 = -1;
//BIO* sockfdSSL;
//SSL_CTX* ssl_ctx;
const char* ca_file;
const char* addrIn;
//const char* addrOut;
//const char* addrOut2;
const char* addrSSL;
const char* port;
const char* portSSL;
const char* topicIn;
//const char* topicOut;
//const char* topicOut2;
const char* topicSSL;
//pthread_t client_daemonOut;
//pthread_t client_daemonOut2;
pthread_t client_daemonIn;
pthread_t client_daemonSSL;

int SSLSent = 0;
int SSLConnect = 0;
int SSLDisconnect = 0;
int watchdogFlag = 0;
int msgId = 1;

char* messageToPublish;
char* messageToPublishAzure;
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */

void sig_handler(int sigio);

/**
 * @brief The function will be called whenever a PUBLISH message is received.
 */
void publish_callback(void** unused, struct mqtt_response_publish *published);
void subscribe_callback(void** unused, struct mqtt_response_publish *published);
//void parse_save(const char* message, size_t size);

/**
 * @brief The client's refresher. This function triggers back-end routines to
 *        handle ingress/egress traffic to the broker.
 *
 * @note All this function needs to do is call \ref __mqtt_recv and
 *       \ref __mqtt_send every so often. I've picked 100 ms meaning that
 *       client ingress/egress traffic will be handled every 100 ms.
 */
//void* publish_client_refresher(void* client);
void* subscribe_client_refresher(void* client);
void* SSLpublish_client_refresher(void* client);
//int makePublisher(void);
//int makePublisher2(void);
int makePublisherSSL(void);
/**
    mosquitto functons
*/
void msq_connect_callback(struct mosquitto* mosq, void* obj, int result);

void msq_publish_callback(struct mosquitto* mosq, void* userdata, int mid);

void msq_disconnect_callback(struct mosquitto* mosq, void* obj, int result);

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


    /* get address (argv[1] if present) */
    if (argc > 1) {
        addrIn = argv[1];
    } else {
        addrIn = ADDR_IN;
        //addr = "test.mosquitto.org";
        //deb = 1;
    }

    /* get port number (argv[2] if present) */
    if (argc > 2) {
        port = argv[2];
    } else {
        port = "1883";
    }

    /* get the topic name to subscribe */
    if (argc > 3) {
        topicIn = argv[3];
    } else {
        topicIn = TOPIC_IN;
    }

    /*get addres to publish if present*/
    /*
    if (argc > 4) {
        addrOut = argv[4];
    } else {
        addrOut = ADDR_OUT;
    }
    */

    /*get the topic name to publish if present*/
    /*
    if (argc > 5) {
        topicOut = argv[5];
    } else {
        topicOut = TOPIC_OUT;
    }
    addrOut2 = ADDR_OUT2;
    topicOut2 = TOPIC_OUT2;
    */
    ca_file = CERTIFICATEFILE;  //SSL certificate
    addrSSL = HOST_SSL;
    portSSL = PORT_SSL_STR;
    topicSSL = TOPIC_SSL;
/****************start mosquitto part *****************************/
    int rc;
    mosquitto_lib_init();
    // create the mosquito object
	struct mosquitto* mosqSSL = mosquitto_new(DEVICEID, false, NULL);
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

	printf("3 Connect returned OK\r\n");


	char mosquittoMess[100];
	sprintf(mosquittoMess,"Test 6 from Kontron N%d",msgId);


	// once connected, we can publish (send) a Telemetry message

	printf("4 Publishing....\r\n");
	SSLSent = 0;
	rc = mosquitto_publish(mosqSSL, &msgId, TOPIC_SSL, strlen(mosquittoMess), mosquittoMess, 1, true);
	if (rc != MOSQ_ERR_SUCCESS)
	{
		mosquitto_error(rc,NULL);
		simple_exit();
	}

	printf("5 Publish returned OK\r\n");

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
    while(SSLConnect == 0){
        //printf("6 Entering Mosquitto Loop...\r\n");
        mosquitto_loop(mosqSSL, -1, 1);
        //sleep(2);
	}
	printf("connected \r\n");
	while(SSLSent == 0){
        //printf("6 Entering Mosquitto Loop...\r\n");
        mosquitto_loop(mosqSSL, -1, 1);
        //sleep(2);
	}
	SSLDisconnect = 0;
    mosquitto_disconnect(mosqSSL);
    while(SSLDisconnect == 0){
        mosquitto_loop(mosqSSL, -1, 1);
    }
    printf("7 Disconnected\r\n");
	//while(1);

/*
//	mosquitto_loop_start(mosqSSL);

	printf("Connecting...\r\n");
	rc = mosquitto_connect(mosqSSL, HOST_SSL, PORT_SSL, 10);
	if (rc != MOSQ_ERR_SUCCESS)
	{
		mosquitto_error(rc,NULL);
		simple_exit();
	}
    sleep(5);
	printf("Connect returned OK\r\n");

	printf("Publishing....\r\n");
	rc = mosquitto_publish(mosqSSL, &msgId, TOPIC_SSL, strlen(messageToPublishAzure), messageToPublishAzure, 1, true);
	if (rc != MOSQ_ERR_SUCCESS)
	{
		printf("Mosquitto publish error\r\n");
		mosquitto_error(rc,NULL);
		simple_exit();
	}
    //sleep(5);
	printf("Publish returned OK\r\n");

	while(SSLSent == 0){
        mosquitto_loop(mosqSSL, -1, 1);
	}

	mosquitto_lib_cleanup();
	printf("7 End mosquitto part\r\n");
	*/
	//mosquitto_disconnect(mosqSSL);
	//while (1);
/*
	if(pthread_create(&client_daemonSSL, NULL, SSLpublish_client_refresher, mosqSSL)) {
        fprintf(stderr, "Failed to start SSLpublisher mosquitto client daemon.\n");
       simple_exit();
    }
*/
/****************stop mosquitto part *****************************/


    /* open the non-blocking TCP socket (connecting to the broker) */
    sockfdIn = open_nb_socket(addrIn, port);

    if(sockfdIn == -1){
        perror("Failed to open input socket: ");
        simple_exit();
    }

    mqtt_init(&clientIn, sockfdIn, sendbufIn, sizeof(sendbufIn), recvbufIn, sizeof(recvbufIn), subscribe_callback);
    mqtt_connect(&clientIn, "subscribing_client", NULL, NULL, 0, NULL, NULL, 0, 400);

    /* check that we don't have any errors */
    if (clientIn.error != MQTT_OK) {
        fprintf(stderr, "connect subscriber error: %s\n", mqtt_error_str(clientIn.error));
        simple_exit();
    }

    /* start a thread to refresh the subscriber client (handle egress and ingree client traffic) */

    if(pthread_create(&client_daemonIn, NULL, subscribe_client_refresher, &clientIn)) {
        fprintf(stderr, "Failed to start subscriber client daemon.\n");
       simple_exit();
    }

    /* subscribe */
    mqtt_subscribe(&clientIn, topicIn, 0);

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

            messageToPublish = parse_get_mess();
            messageToPublishAzure = parse_get_mess_azure();
            if(messageToPublish != NULL)  //there is a mesasage to publish
            {
    /***************************************************mosquitto part***************************************************************/
                    printf("Start.\n\r");
                    msgId++;

                    SSLConnect = 0;

                    rc = mosquitto_connect(mosqSSL, HOST_SSL, PORT_SSL, 10);
                    if (rc != MOSQ_ERR_SUCCESS)
                    {
                        mosquitto_error(rc,NULL);
                        printf("Exit from connection.\n\r");
                        break;//simple_exit();
                    }
                    while(SSLConnect == 0){
                        mosquitto_loop(mosqSSL, -1, 1);
                    }

                    SSLSent = 0;
                    printf("Publishing N%d message\n\r",msgId);
                    rc = mosquitto_publish(mosqSSL, &msgId, TOPIC_SSL, strlen(messageToPublishAzure), messageToPublishAzure, 1, true);
                    if (rc != MOSQ_ERR_SUCCESS){
                        printf("Mosquitto error, can't send N%d message\n\r",msgId);
                        mosquitto_error(rc,NULL);
                        printf("Exit from publishing.\n\r");
                        break;//simple_exit();
                        //exit_example(EXIT_SUCCESS, sockfdIn, sockfdOut, sockfdOut2, &client_daemonIn, NULL,NULL);
                    }
                    while(SSLSent == 0){
                        mosquitto_loop(mosqSSL, -1, 1);
                    }
                    printf("N%d message is published.\n\r",msgId);
                                                                    /**  <--- после вывода этого сообщения виснет*/
                    SSLDisconnect = 0;
                    //sleep(1);
                    rc = mosquitto_disconnect(mosqSSL);
                    if (rc != MOSQ_ERR_SUCCESS)
                    {
                        mosquitto_error(rc,NULL);
                        printf("Exit from disconnection.\n\r");
                        break;//simple_exit();
                    }
                    //sleep(1);
                    while(SSLDisconnect == 0){
                        mosquitto_loop(mosqSSL, -1, 1);
                    }
                    printf("Azure disconnected.\n\r");
                    sleep(1);
                    printf("Delay.\n\r");
    /************************************************end mosquitto part***********************************************************/
            }
        }
        if((quit_sig != 1) && (exit_sig != 1)){
            printf("Connection lost, try to reconnect.\n\r");
            sleep(10); //try to reconnect
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
/**
int makePublisher(void){

    sockfdOut = open_nb_socket(addrOut, port);

    if(sockfdOut == -1){
        perror("Failed to open output socket: ");
        //exit_example(EXIT_FAILURE, sockfdIn, sockfdOut, NULL, NULL);
        return -1;
    }
    mqtt_init(&clientOut, sockfdOut, sendbufOut, sizeof(sendbufOut), recvbufOut, sizeof(recvbufOut), publish_callback);
    mqtt_connect(&clientOut, "publishing_client1", NULL, NULL, 0, NULL, NULL, 0, 400);

    // check that we don't have any errors
    if (clientOut.error != MQTT_OK) {
        fprintf(stderr, "connect publisher error: %s\n", mqtt_error_str(clientOut.error));
        //exit_example(EXIT_FAILURE, sockfdIn, sockfdOut,NULL,NULL);
        if (sockfdOut != -1) close(sockfdOut);
        return -1;

    }
    return 0;
      //start a thread to refresh the publisher client (handle egress and ingree client traffic)
//
//    if(pthread_create(&client_daemonOut, NULL, publish_client_refresher, &clientOut)) {
//        fprintf(stderr, "Failed to start publisher client daemon.\n");
//        //exit_example(EXIT_FAILURE, sockfdIn, sockfdOut,NULL, NULL);
//        if (sockfdOut != -1) close(sockfdOut);
//        return;
//    }

}
*/
/*
int makePublisher2(void){

    sockfdOut2 = open_nb_socket(addrOut2, port);

    if(sockfdOut2 == -1){
        perror("Failed to open output2 socket: ");
        //exit_example(EXIT_FAILURE, sockfdIn, sockfdOut, NULL, NULL);
        return -1;
    }
    mqtt_init(&clientOut2, sockfdOut2, sendbufOut2, sizeof(sendbufOut2), recvbufOut2, sizeof(recvbufOut2), publish_callback);
    mqtt_connect(&clientOut2, "publishing_client2", NULL, NULL, 0, NULL, NULL, 0, 400);

    // check that we don't have any errors
    if (clientOut2.error != MQTT_OK) {
        fprintf(stderr, "connect publisher2 error: %s\n", mqtt_error_str(clientOut2.error));
        //exit_example(EXIT_FAILURE, sockfdIn, sockfdOut,NULL,NULL);
        if (sockfdOut2 != -1) close(sockfdOut2);
        return -1;

    }
    return 0;
     // start a thread to refresh the publisher client (handle egress and ingree client traffic)
//
//    if(pthread_create(&client_daemonOut, NULL, publish_client_refresher, &clientOut)) {
//        fprintf(stderr, "Failed to start publisher client daemon.\n");
//        //exit_example(EXIT_FAILURE, sockfdIn, sockfdOut,NULL, NULL);
//        if (sockfdOut != -1) close(sockfdOut);
//        return;
//    }

}

*/

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
    //if (client_daemonOut != NULL) pthread_cancel(*client_daemonOut);
    //if (client_daemonOut2 != NULL) pthread_cancel(*client_daemonOut2);
   // pthread_cancel(client_daemonSSL);
    mosquitto_lib_cleanup();
    exit(status);
}

void simple_exit(void)
{
    exit_example(EXIT_SUCCESS, sockfdIn, &client_daemonIn, NULL,NULL);
}

void publish_callback(void** unused, struct mqtt_response_publish *published)
{
     /* not used in this example */
}


void subscribe_callback(void** unused, struct mqtt_response_publish *published)
{
    /* note that published->topic_name is NOT null-terminated (here we'll change it to a c-string) */
    char* topic_name = (char*) malloc(published->topic_name_size + 1);
    memcpy(topic_name, published->topic_name, published->topic_name_size);
    topic_name[published->topic_name_size] = '\0';

    /*save only 32 bytes message*/
    if(published->application_message_size == 89)parse_save((const char*) published->application_message,published->application_message_size);

    //printf("Received publish from '%s' : %s\n", topic_name, (const char*) published->application_message);

    free(topic_name);
}

void* publish_client_refresher(void* client)
{
    MQTTErrors_t error;
    while(1)
    {
        error = mqtt_sync((struct mqtt_client*) client);
        if( error != MQTT_OK){
            fprintf(stderr, "publisher error: %s\n", mqtt_error_str(error));
        }
        usleep(100000U);
    }
    return NULL;
}
void* subscribe_client_refresher(void* client)
{
    while(1)
    {
        mqtt_sync((struct mqtt_client*) client);
        usleep(100000U);
    }
    return NULL;
}

void* SSLpublish_client_refresher(void* client)
{
    /**
    while(1)
    {
        mosquitto_loop((struct mosquitto*)client, -1, 1);
        usleep(100000U);
    }
    return NULL;
    */
    mosquitto_loop_forever((struct mosquitto*)client, -1, 1);
    return NULL;
}

void* watchdog_refresher(void* client)
{

    while(1)
    {
        watchdogFlag = 0;
        sleep(60);
        if(watchdogFlag == 0){
             simple_exit();
        }
    }
    return NULL;
}

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
    }
    SSLDisconnect = 1;
}

