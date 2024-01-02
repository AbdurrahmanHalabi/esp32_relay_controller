#ifndef __HTTP_REQUEST_H__
#define __HTTP_REQUEST_H__


/*******************************************************
 *                Function Declarationss
 *******************************************************/
void https_auth_request(void);
void https_meters_request(void);
void https_post_request(char*);
void ota_task(void *pvParameter);
//void activate_tasks(void);

#endif /* __HTTP_REQUEST_H__ */
