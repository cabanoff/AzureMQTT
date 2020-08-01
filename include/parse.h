#ifndef __PARSE_H__
#define __PARSE_H__



/**
 * @brief The function saves published message
 * @param[in] message reference to received message.
 *
 * @param[in] size size of the received message.
 *
 * @returns none
 */
 void parse_save(const char* message, size_t size);


/**
 * @brief returns prepared message
 *
 * @param[in] message string for copy to,
 * @param[in] len - length of this string
 *
 * @returns message to be sent
 */
 char* parse_get_mess(char* message,unsigned int len);

 char* parse_get_mess_azure(void);

 /**
 * @brief initialization deinitialization mutex
 *
 *
 */
 void parse_init(void);
 void parse_deinit(void);


#endif

