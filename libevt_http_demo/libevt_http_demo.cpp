// libevt_http_demo.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"


#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <evhttp.h>
#include <sstream>
#include <Winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <map>

#if (defined WIN32) ||  (defined _WIN32)
#include<Windows.h>
#else
#include<unistd.h> 
#include<sys/types.h>
#include<strings.h>
#endif

#include <limits.h>
#include <locale.h>

#include "workqueue.h"
#include "libxl.h"
#include ".\json\tinyjson.h"
//#include ".\json\CJsonObject\CJsonObject.hpp"
#pragma comment(lib, "libxl.lib")
using namespace std;
using namespace libxl;
//using namespace tiny;
using namespace Json;

class CXlsDataExp
{
public:
	CXlsDataExp(const wchar_t* strFile) {
		m_pbook = xlCreateBook();//创建一个二进制格式的XLS（Execl97-03）的实例,在使用前必须先调用这个函数创建操作excel的对象		
		if (strFile)
		{
			m_pbook->load(strFile);
		}
	};


	~CXlsDataExp() {
		Close();
	};

	int GetRecCount(const wstring & strSheet)
	{
		for (int i = 0; i < m_pbook->sheetCount(); i++)
		{
			Sheet* sheet = m_pbook->getSheet(i);
			if (sheet)
			{
				if (sheet->name() == strSheet)
				{
					int nLast = sheet->lastRow();
					if (nLast > 0)
					{
						nLast--;
					}
					return nLast;
				}
			}
		}
		return 0;
	}

	bool WriteRec(const wstring & strSheet, const vector<wstring> & rec)
	{
		//book->setKey(......);//如果购买了该库，则设置相应的key，若没有购买，则不用这行
		if (!m_pbook)//是否创建实例成功
		{
			return false;
		}

		for (int i = 0; i < m_pbook->sheetCount(); i++)
		{
			Sheet* sheet = m_pbook->getSheet(i);
			if (sheet)
			{
				if (sheet->name() == strSheet)
				{
					m_sheetMap[strSheet] = sheet;
					break;
				}
			}
		}

		Sheet* sheet = NULL;
		if (m_sheetMap.count(strSheet))
		{
			sheet = m_sheetMap[strSheet];
		}
		else
		{
			sheet = m_pbook->addSheet(strSheet.c_str());//添加一个工作表
			m_sheetMap[strSheet] = sheet;
		}

		//一个excel文件既是一个工作簿，你可以把工作簿看作是一个本子，而本子是由一页一页的纸张装订在一起的，excel中的sheet就是这些纸张。
		if (sheet)
		{
			int nLast = sheet->lastRow();
			int ncolCnt = rec.size();

			for (int i = 0; i < ncolCnt; i++)
			{
				sheet->writeStr(nLast, i, rec[i].c_str());//在第二行 第二列（B列）的表格中写入字符串"Hello, World !"。程序中从0开始计数。第0行就是execl的第1行
			}
		}
	};

	void Save(const wchar_t* strFile)
	{
		if (m_pbook->save(strFile))//保存到example.xls
		{
			//.....
		}
		else
		{
			//std::cout << m_pbook->errorMessage() << std::endl;
		}
	};

	void Close() {
		if (m_pbook)
		{
			m_pbook->release();//释放对象！！！！！
			m_pbook = 0;
		}
	}
protected:
	Book* m_pbook;
	map<wstring, Sheet*>	m_sheetMap;
};


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

//定义 shared_ptr<T> 的智能指针
#define _PRTO(T, N, ...)  std::shared_ptr<T> N(new T(##__VA_ARGS__))
//定义 shared_ptr<T> 的数组智能指针
#define _PRTA(T, N, n)    std::shared_ptr<T> N(new T[n])



int wc2mbs(wchar_t *wc, unsigned int wc_size, char *mbs)
{
	int mbs_size;
#if (defined WIN32) ||  (defined _WIN32)
	setlocale(LC_ALL, "chs");
#else
	setlocale(LC_ALL, "zh_CN.gbk");
#endif

	if (wc_size == 0)
		wc_size = UINT_MAX;

	mbs_size = wcstombs(0, wc, wc_size);

	if (mbs != 0)
		mbs_size = wcstombs(mbs, wc, wc_size);

	return mbs_size;
}

int mbs2wc(char *mbs, unsigned int mbs_size, wchar_t *wc)
{
	int wc_size;
#if (defined WIN32) ||  (defined _WIN32)
	setlocale(LC_ALL, "chs");
#else
	setlocale(LC_ALL, "zh_CN.gbk");
#endif

	if (mbs_size == 0)
		mbs_size = UINT_MAX;

	wc_size = mbstowcs(0, mbs, mbs_size);

	if (wc != 0)
		wc_size = mbstowcs(wc, mbs, mbs_size);

	return wc_size;
}


CXlsDataExp g_xlsfile(L"d:\\1.xls");

void SaveJsonData(const char * jsData)
{
	JObject objdata = JObject::Deserialize(jsData);

	if (!objdata.Exists(L"Pagenum"))
	{
		return;
	}

	auto data =objdata[L"Data"];
	int  ncnt = data.Count();
	for (int i = 0; i < ncnt; i++)
	{
		vector<wstring> rec;
		auto qishu = data[i];
		{
			auto qs = qishu[L"qishu"];
			std::wstringstream ss;
			ss << qs.ToString().c_str();
			rec.push_back(ss.str());
		}
		{
			auto qs = qishu[L"adddate"];
			std::wstringstream ss;
			ss << qs.ToString().c_str();
			rec.push_back(ss.str());
		}
		auto & balls = qishu[L"balls"];
		int nballcnt = balls.Count();
		for (int j = 0; j < nballcnt; j++)
		{
			wstring s = balls[j][L"num"].ToString();

			std::wstringstream ss;
			ss << s;
			rec.push_back(ss.str());
		}
		g_xlsfile.WriteRec(L"data", rec);
	}

	g_xlsfile.Save(L"d:\\1.xls");
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
			int nlen = EVBUFFER_LENGTH(req->input_buffer);
			post_data[nlen] = '\0';
			//_PRTA(wchar_t, pdata, nlen + 1);
			//memset(pdata.get(), 0, (nlen + 1)*sizeof(wchar_t));
			//mbs2wc(post_data, nlen, pdata.get());
			SaveJsonData(post_data);
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
#include <assert.h>
#include <io.h>
int main(int argc, char* argv[])
{
#ifdef WIN32
	init_win_socket();
#endif

	FILE * fp = NULL;
//	fp = fopen("D:\\code\\libevt_httpsvr\\Debug\\jsdata.txt", "rb");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		int nl = ftell(fp);
		char * dt = new char[nl + 1];
		memset(dt, 0, nl + 1);
		fseek(fp, 0, SEEK_SET);
		int r =fread(dt, 1, nl, fp);
		assert(r == nl);
		fclose(fp);


		int nlen = nl;
		_PRTA(wchar_t, pdata, nlen + 1);
		memset(pdata.get(), 0, (nlen + 1) * sizeof(wchar_t));
		mbs2wc(dt, nlen, pdata.get());
		int n = strlen(dt);
		SaveJsonData(dt);
	}



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
