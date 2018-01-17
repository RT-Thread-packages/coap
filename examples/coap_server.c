/*
* File      : coap_server.c
* This file is part of RT-Thread RTOS
* COPYRIGHT (C) 2009-2018 RT-Thread Develop Team
*/

#include <rtthread.h>
#include "finsh.h"

#include <string.h>
#include <netdb.h>

#include "coap.h"

#define COAP_DEFAULT_TIME_SEC 5
#define COAP_DEFAULT_TIME_USEC 0

static coap_async_state_t *async = NULL;

static void send_async_response(coap_context_t *ctx, const coap_endpoint_t *local_if)
{
    coap_pdu_t *response;
    unsigned char buf[3];
    const char *response_data = "Hello rtthread!";
    size_t size = sizeof(coap_hdr_t) + strlen(response_data) + 20;
    response = coap_pdu_init(async->flags & COAP_MESSAGE_CON, COAP_RESPONSE_CODE(205), 0, size);
    response->hdr->id = coap_new_message_id(ctx);
    if (async->tokenlen)
        coap_add_token(response, async->tokenlen, async->token);
    coap_add_option(response, COAP_OPTION_CONTENT_TYPE, coap_encode_var_bytes(buf, COAP_MEDIATYPE_TEXT_PLAIN), buf);

    coap_add_data(response, strlen(response_data), (unsigned char *)response_data);

    if (coap_send(ctx, local_if, &async->peer, response) == COAP_INVALID_TID)
    {
        rt_kprintf("Error: coap send to client return error.\n");
    }
    coap_delete_pdu(response);
    coap_async_state_t *tmp;
    coap_remove_async(ctx, async->id, &tmp);
    coap_free_async(async);
    async = NULL;
}

static void async_handler(coap_context_t *ctx, struct coap_resource_t *resource,
              const coap_endpoint_t *local_interface, coap_address_t *peer,
              coap_pdu_t *request, str *token, coap_pdu_t *response)
{
    async = coap_register_async(ctx, peer, request, COAP_ASYNC_SEPARATE | COAP_ASYNC_CONFIRM, (void *)"#$ no data #$");
}

static void coap_server_task(void *arg)
{
    coap_context_t *ctx = NULL;
    coap_address_t serv_addr;
    coap_resource_t *resource = NULL;
    fd_set readfds;
    struct timeval tv;
    int flags = 0;

    for (;;)
    {
        /* Prepare the CoAP server socket */
        coap_address_init(&serv_addr);
        serv_addr.addr.sin.sin_family = AF_INET;
        serv_addr.addr.sin.sin_addr.s_addr = INADDR_ANY;
        serv_addr.addr.sin.sin_port = htons(COAP_DEFAULT_PORT);

        ctx = coap_new_context(&serv_addr);
        if (ctx)
        {
            tv.tv_usec = COAP_DEFAULT_TIME_USEC;
            tv.tv_sec = COAP_DEFAULT_TIME_SEC;

            /* Initialize the resource, first param is uri */
            resource = coap_resource_init((unsigned char *)"rtthread", 8, 0);
            if (resource)
            {
                coap_register_handler(resource, COAP_REQUEST_GET, async_handler);
                coap_add_resource(ctx, resource);

                /*For incoming connections*/
                for (;;)
                {
                    FD_ZERO(&readfds);
                    FD_CLR(ctx->sockfd, &readfds);
                    FD_SET(ctx->sockfd, &readfds);

                    int result = select(ctx->sockfd + 1, &readfds, 0, 0, &tv);
                    if (result > 0)
                    {
                        if (FD_ISSET(ctx->sockfd, &readfds))
                            coap_read(ctx);
                    }
                    else if (result < 0)
                    {
                        break;
                    }
                    else
                    {
                        rt_kprintf("select waiting...\n");
                    }

                    if (async)
                    {
                        send_async_response(ctx, ctx->endpoint);
                    }
                }
            }

            coap_free_context(ctx);
        }
    }
}

static int coap_server(void)
{
    rt_thread_t tid;

    tid = rt_thread_create("CoAPServer",
                           coap_server_task, RT_NULL,
                           4096,
                           RT_THREAD_PRIORITY_MAX - 2,
                           10);
    if (tid != RT_NULL)
        rt_thread_startup(tid);

    return 0;        
}
MSH_CMD_EXPORT(coap_server, Start a coap server No input param);
