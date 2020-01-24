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
 * @brief returns first message in stack.
 *
 * @param[in] pointer to the thethings.io mqtt message, and azure mqtt message
 *
 * @returns NULL if no mesages stored
 */
 char* parse_get_mess(void);
 char* parse_get_mess_azure(void);

#endif

