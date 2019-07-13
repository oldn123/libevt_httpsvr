// libevt_http_demo.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"


#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <evhttp.h>

#include <Winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <map>

#include "workqueue.h"
#include ".\json\tinyjson.h"


static workqueue_t workqueue;

using namespace std;

int init_win_socket()
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		return -1;
	}
	return 0;
}



void OnResponse(evhttp_request * req, void * arg)
{
	try {
		switch (req->type)
		{
		case EVHTTP_REQ_POST: {
			char* post_data = ((char*)EVBUFFER_DATA(req->input_buffer));
			if (!post_data) {
				int nl = EVBUFFER_LENGTH(req->input_buffer);
				post_data = (char*)evbuffer_pullup(req->input_buffer, nl);
				if (!post_data)
				{
					return;
				}
			}
			post_data[EVBUFFER_LENGTH(req->input_buffer)] = '\0';
		}
		break;
		case EVHTTP_REQ_GET: {
			/* 以下为测试代码 */
			//evbuffer* buf = evbuffer_new();
			//JsonOperate json_operator;
			evkeyvalq keyvalq;
			const char *req_str = evhttp_request_get_uri(req);
			evhttp_parse_query(req_str, &keyvalq);
			evkeyval *val = keyvalq.tqh_first;
			std::map<string, string> dt_map;
			while (val) {
				dt_map[val->key] = val->value;
				val = val->next.tqe_next;
			}
		}
		break;
		default:
			break;
		}

	}
	catch (std::exception& e) {
		OutputDebugStringA(e.what());
	}


	struct evbuffer *buf = evbuffer_new();
	if (!buf)
	{
		puts("failed to create response buffer \n");
		return;
	}

	const char * s_uri = evhttp_request_get_uri(req);
	printf("Server Responsed. Requested: %s\n", s_uri);
	const struct evhttp_uri* uri = evhttp_request_get_evhttp_uri(req);
	const char* query_str = evhttp_uri_get_query(uri);

	struct evkeyvalq *Req_Header;
	Req_Header = evhttp_request_get_input_headers(req);
	printf(" ========header---->\n");
	for (evkeyval* header = Req_Header->tqh_first; header; header = header->next.tqe_next)
	{
		printf(" %s: %s\n", header->key, header->value);
	}
	printf(" --------->header==========\n");

	struct evbuffer  *req_evb = evhttp_request_get_input_buffer(req);
	int req_len = evbuffer_get_length(req_evb);


	struct evkeyvalq params;
	evhttp_parse_query(s_uri, &params);

	//获取POST方法的数据
	char *post_data = (char *)EVBUFFER_DATA(req->input_buffer);
	printf("post_data=%s\n", post_data);



	//evbuffer_add_printf(buf, "Server Responsed. Requested: %s\n", evhttp_request_get_uri(req));
	evhttp_send_reply(req, HTTP_OK, "OK", buf);
	evbuffer_free(buf);
}

static void on_client_request_function(struct job *job) {
	struct evhttp_request *client = (struct evhttp_request *)job->user_data;
	OnResponse(client, NULL);
	delete job;
}

void generic_handler(struct evhttp_request *req, void *arg)
{
	//收到请求即转到线程处理
	workqueue_t * workqueue = (workqueue_t *)arg;
	job_t * job = new job_t;
	if ( job == NULL) {
		return;
	}
	job->job_function = on_client_request_function;
	job->user_data = req;
	workqueue_add_job(workqueue, job);

	return;
}



#define NUM_THREADS 8
void initThread()
{
	if (workqueue_init(&workqueue, NUM_THREADS)) {
		perror("Failed to create work queue");
		workqueue_shutdown(&workqueue);
	}
}


int main(int argc, char* argv[])
{
#ifdef WIN32
	init_win_socket();
#endif


	short          http_port = 8899;
	char          *http_addr = "127.0.0.1";

	struct event_base * base = event_base_new();

	struct evhttp * http_server = evhttp_new(base);
	if (!http_server)
	{
		return -1;
	}

	int ret = evhttp_bind_socket(http_server, http_addr, http_port);
	if (ret != 0)
	{
		return -1;
	}



	std::thread threadwork(initThread);
	threadwork.detach();

	evhttp_set_gencb(http_server, generic_handler, &workqueue);

	//evhttp_set_cb(http_server, "/", generic_handler , NULL);


	printf("http server start OK! \n");

	event_base_dispatch(base);

	evhttp_free(http_server);

	WSACleanup();
	return 0;
}
