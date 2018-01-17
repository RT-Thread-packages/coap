/*
* File      : coap_client.c
* This file is part of RT-Thread RTOS
* COPYRIGHT (C) 2009-2018 RT-Thread Develop Team
*/

#include <rtthread.h>
#include "finsh.h"

#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#include "coap.h"
#include "coap_config.h"

#define COAP_DEFAULT_DEMO_URI "coap://coap.me/test"

#define COAP_DEFAULT_TIME_SEC 5
#define COAP_DEFAULT_TIME_USEC 0

static uint8_t coap_request_mode = 0;
static char coap_uri[64] = {0};
static coap_uri_t uri;

static void message_handler(struct coap_context_t *ctx, const coap_endpoint_t *local_interface, const coap_address_t *remote,
                            coap_pdu_t *sent, coap_pdu_t *received,
                            const coap_tid_t id)
{
    unsigned char *data = NULL;
    size_t data_len;
    coap_opt_t *block_opt;
    coap_pdu_t *request = NULL;
    unsigned char buf[4];
    coap_opt_iterator_t opt_iter;

    coap_tid_t tid;

    if (COAP_RESPONSE_CLASS(received->hdr->code) == 2)
    {

        if (coap_get_data(received, &data_len, &data))
        {
            rt_kprintf("%.*s", data_len, data);
        }
        /* Got some data, check if block option is set. Behavior is undefined if
     * both, Block1 and Block2 are present. */
        block_opt = coap_check_option(received, COAP_OPTION_BLOCK2, &opt_iter);
        if (block_opt)
        { /* handle Block2 */
            unsigned short blktype = opt_iter.type;
            if (COAP_OPT_BLOCK_MORE(block_opt))
            {
                request = coap_new_pdu();

                if (request)
                {
                    // CoAP message type
                    request->hdr->type = COAP_MESSAGE_CON;
                    request->hdr->id = coap_new_message_id(ctx);

                    // CoAP request methods
                    request->hdr->code = coap_request_mode;

                    coap_add_option(request, COAP_OPTION_URI_PATH, uri.path.length, uri.path.s);

                    coap_add_option(request,
                                    blktype,
                                    coap_encode_var_bytes(buf,
                                                          ((coap_opt_block_num(block_opt) + 1) << 4) |
                                                              COAP_OPT_BLOCK_SZX(block_opt)),
                                    buf);

                    tid = coap_send_confirmed(ctx, local_interface, remote, request);
                    if (tid == COAP_INVALID_TID)
                    {
                        rt_kprintf("message_handler: error sending new request\n");
                        coap_delete_pdu(request);
                        return;
                    }
                }
            }
        }
    }
}

static void coap_client_task(void *arg)
{
    struct hostent *hp;
    struct ip4_addr *ip4_addr;

    coap_tick_t now;

    coap_context_t *ctx = NULL;
    coap_address_t dst_addr, src_addr;

    fd_set readfds;
    struct timeval tv;
    int result;
    coap_pdu_t *request = NULL;

    const char *server_uri = RT_NULL;
    char server_host[64] = {0};

    if (strlen(coap_uri) != 0)
        server_uri = coap_uri;
    else
        server_uri = COAP_DEFAULT_DEMO_URI;

    /* CoAP request methods */
    uint8_t get_method = coap_request_mode; // COAP_REQUEST_GET

    if (coap_split_uri((const uint8_t *)server_uri, strlen(server_uri), &uri) == -1)
    {
        rt_kprintf("CoAP server uri error\n");
        goto _exit;
    }

    char *str_s = (char *)uri.host.s;
    char *dlm_s = (char *)"/";
    char *str_t = RT_NULL, *saveptr = RT_NULL;

    str_t = strtok_r(str_s, dlm_s, &saveptr);

    if (str_t != RT_NULL)
    {
        rt_memset(server_host, 0, sizeof(server_host));
        strncpy((char *)server_host, str_t, sizeof(server_host));
        rt_kprintf("server_host = %s\n", server_host);
    }
    else
    {
        strncpy((char *)server_host, (const char *)uri.host.s, sizeof(server_host));
        rt_kprintf("server_host = %s\n", server_host);
    }

    while (1)
    {

        // It may take several iterations for getting DNS success
        hp = gethostbyname((const char *)server_host); // getaddrinfo
        if (hp == NULL)
        {
            rt_kprintf("DNS lookup failed\n");
            rt_thread_delay(1000);
            continue; // Duplicate DNS GET
        }

        ip4_addr = (struct ip4_addr *)hp->h_addr;
        rt_kprintf("DNS lookup succeeded. IP=%s\n", inet_ntoa(*ip4_addr));

        // Reset the given coap_address_t object to its default values
        coap_address_init(&src_addr);
        src_addr.addr.sin.sin_family = AF_INET;
        src_addr.addr.sin.sin_port = htons(0);
        src_addr.addr.sin.sin_addr.s_addr = INADDR_ANY;

        // Create a new object for holding coap stack status
        ctx = coap_new_context(&src_addr);
        if (ctx)
        {
            // Init destination addr
            coap_address_init(&dst_addr);
            dst_addr.addr.sin.sin_family = AF_INET;
            dst_addr.addr.sin.sin_port = htons(COAP_DEFAULT_PORT);
            dst_addr.addr.sin.sin_addr.s_addr = ip4_addr->addr;

            coap_register_option(ctx, COAP_OPTION_BLOCK2);

            // Register a received message handler
            coap_register_response_handler(ctx, message_handler);

            request = coap_new_pdu();
            if (request)
            {
                // CoAP message type
                request->hdr->type = COAP_MESSAGE_CON;
                request->hdr->id = coap_new_message_id(ctx);
                // CoAP request methods
                request->hdr->code = get_method;

                if (uri.path.length != 0)
                {
                    coap_add_option(request, COAP_OPTION_URI_PATH, uri.path.length, uri.path.s);
                }

                // Sends a confirmed CoAP message to given destination
                int coap_ret = coap_send_confirmed(ctx, ctx->endpoint, &dst_addr, request);

                tv.tv_usec = COAP_DEFAULT_TIME_USEC;
                tv.tv_sec = COAP_DEFAULT_TIME_SEC;

                coap_queue_t *nextpdu;

                for (;;)
                {
                    FD_ZERO(&readfds);
                    FD_SET(ctx->sockfd, &readfds);

                    nextpdu = coap_peek_next(ctx);
                    coap_ticks(&now);

                    while (nextpdu && nextpdu->t <= now - ctx->sendqueue_basetime)
                    {
                        coap_retransmit(ctx, coap_pop_next(ctx));

                        rt_kprintf("\nCoAP retransmit...\n");
                        nextpdu = coap_peek_next(ctx);
                    }

                    if (!nextpdu)
                    {
                        rt_kprintf("\nTransmit end.\n");
                        goto _exit;
                    }

                    result = select(ctx->sockfd + 1, &readfds, RT_NULL, RT_NULL, &tv);
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
                        rt_kprintf("\nselect timeout...\n");
                    }
                }
            }
        }

        goto _exit;
    }

_exit:
    if (ctx != RT_NULL)
        coap_free_context(ctx);
    return;
}

static void usage(const char *program, const char *version)
{
    rt_kprintf("\nHelp:\nlibcoap %s -- a small CoAP implementation\n"
               "usage: %s [-m type...] [Uri text]\n",
               version, program);
    rt_kprintf("\t\t type: get; post; put; delete.\n"
               "Example: %s -m get coap://wsncoap.org\n",
               program);
}

static uint8_t cmdline_method(char *arg)
{
    static char *methods[] = {0, "get", "post", "put", "delete", 0};
    unsigned char i;

    for (i = 1; methods[i] && strcasecmp(arg, methods[i]) != 0; ++i)
        ;

    return i; /* note that we do not prevent illegal methods */
}

static int cmdline_option_parser(int argc, char **argv)
{
    int8_t ret = -1;

    if (argc < 4)
    {
        rt_kprintf("\n======= CoAP client usage: ========\n");
        usage(argv[0], PACKAGE_VERSION);
        return ret;
    }
    else if (strstr(argv[3], "coap://") == RT_NULL) // judge uri format
    {
        rt_kprintf("Warning: param three is not allowed!\n");
        usage(argv[0], PACKAGE_VERSION);
        return ret;
    }
    else if (strcmp(argv[1], "-m") != 0)
    {
        rt_kprintf("Warning: in param is error, please use '-m' param.\n");
        usage(argv[0], PACKAGE_VERSION);
        return ret;
    }

    coap_request_mode = cmdline_method(argv[2]); //1:GET; 2:POST; 3:PUT; 4:DELETE

    if (strlen(argv[3]) < sizeof(coap_uri))
        strncpy((char *)coap_uri, (const char *)argv[3], sizeof(coap_uri));
    else
    {
        rt_kprintf("Warning: coap uri is too long. The length of the limit is %d\n", sizeof(coap_uri) - 1);
        return ret;
    }

    rt_kprintf("coap uri = %s\n", coap_uri);

    ret = 0;
    return ret;
}

static int coap_client(int argc, char **argv)
{
    rt_thread_t tid;
    int8_t ret = 0;

    ret = cmdline_option_parser(argc, argv);
    if (ret != 0)
        return ret;

    tid = rt_thread_create("CoAPClient",
                           coap_client_task, RT_NULL,
                           4096,
                           RT_THREAD_PRIORITY_MAX - 2,
                           10);
    if (tid != RT_NULL)
        rt_thread_startup(tid);

    return ret;
}
MSH_CMD_EXPORT(coap_client, Using coap_client to get usage);
