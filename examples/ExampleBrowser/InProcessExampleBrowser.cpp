#include "InProcessExampleBrowser.h"

//#define EXAMPLE_CONSOLE_ONLY
#ifdef EXAMPLE_CONSOLE_ONLY
	#include "EmptyBrowser.h"
	typedef EmptyBrowser DefaultBrowser;
#else
	#include "OpenGLExampleBrowser.h"
	typedef OpenGLExampleBrowser DefaultBrowser;
#endif //EXAMPLE_CONSOLE_ONLY

#include "Bullet3Common/b3CommandLineArgs.h"
#include "../Utils/b3Clock.h"

#include "ExampleEntries.h"
#include "Bullet3Common/b3Logging.h"

void	ExampleBrowserThreadFunc(void* userPtr,void* lsMemory);
void*	ExampleBrowserMemoryFunc();

#include <stdio.h>
//#include "BulletMultiThreaded/PlatformDefinitions.h"

#ifndef _WIN32
#include "../MultiThreading/b3PosixThreadSupport.h"

static b3ThreadSupportInterface* createExampleBrowserThreadSupport(int numThreads)
{
	b3PosixThreadSupport::ThreadConstructionInfo constructionInfo("testThreads",
                                                                ExampleBrowserThreadFunc,
                                                                ExampleBrowserMemoryFunc,
                                                                numThreads);
    b3ThreadSupportInterface* threadSupport = new b3PosixThreadSupport(constructionInfo);

	return threadSupport;

}


#elif defined( _WIN32)
#include "../MultiThreading/b3Win32ThreadSupport.h"

b3ThreadSupportInterface* createExampleBrowserThreadSupport(int numThreads)
{
	b3Win32ThreadSupport::Win32ThreadConstructionInfo threadConstructionInfo("testThreads",ExampleBrowserThreadFunc,ExampleBrowserMemoryFunc,numThreads);
	b3Win32ThreadSupport* threadSupport = new b3Win32ThreadSupport(threadConstructionInfo);
	return threadSupport;

}
#endif



struct	ExampleBrowserArgs
{
	ExampleBrowserArgs()
		:m_fakeWork(1),m_argc(0)
	{
	}
	b3CriticalSection* m_cs;
	float m_fakeWork;
  int m_argc;
  char** m_argv;
};

struct ExampleBrowserThreadLocalStorage
{
	int threadId;
};

enum TestExampleBrowserCommunicationEnums
{
	eRequestTerminateExampleBrowser = 13,
	eExampleBrowserHasTerminated
};

void	ExampleBrowserThreadFunc(void* userPtr,void* lsMemory)
{
	printf("thread started\n");

	ExampleBrowserThreadLocalStorage* localStorage = (ExampleBrowserThreadLocalStorage*) lsMemory;

	ExampleBrowserArgs* args = (ExampleBrowserArgs*) userPtr;
	int workLeft = true;
  b3CommandLineArgs args2(args->m_argc,args->m_argv);
	b3Clock clock;
	
	
	ExampleEntries examples;
	examples.initExampleEntries();

	ExampleBrowserInterface* exampleBrowser = new DefaultBrowser(&examples);
	bool init = exampleBrowser->init(args->m_argc,args->m_argv);
	clock.reset();
	if (init)
	{
		do 
		{
			float deltaTimeInSeconds = clock.getTimeMicroseconds()/1000000.f;
			clock.reset();
			exampleBrowser->update(deltaTimeInSeconds);

		} while (!exampleBrowser->requestedExit() && (args->m_cs->getSharedParam(0)!=eRequestTerminateExampleBrowser));
	}
	delete exampleBrowser;
	args->m_cs->lock();
	args->m_cs->setSharedParam(0,eExampleBrowserHasTerminated);
	args->m_cs->unlock();
	printf("finished\n");
	//do nothing
}


void*	ExampleBrowserMemoryFunc()
{
	//don't create local store memory, just return 0
	return new ExampleBrowserThreadLocalStorage;
}





struct btInProcessExampleBrowserInternalData
{
	ExampleBrowserArgs m_args;
	b3ThreadSupportInterface* m_threadSupport;
};


btInProcessExampleBrowserInternalData* btCreateInProcessExampleBrowser(int argc,char** argv2)
{

	btInProcessExampleBrowserInternalData* data = new btInProcessExampleBrowserInternalData;

	int numThreads = 1;
	int i;

	data->m_threadSupport = createExampleBrowserThreadSupport(numThreads);

	printf("argc=%d\n", argc);
	for (i=0;i<argc;i++)
	{
		printf("argv[%d] = %s\n",i,argv2[i]);
	}

	for (i=0;i<data->m_threadSupport->getNumTasks();i++)
	{
		ExampleBrowserThreadLocalStorage* storage = (ExampleBrowserThreadLocalStorage*) data->m_threadSupport->getThreadLocalMemory(i);
		b3Assert(storage);
		storage->threadId = i;
	}


	data->m_args.m_cs = data->m_threadSupport->createCriticalSection();
	data->m_args.m_cs->setSharedParam(0,100);
 	data->m_args.m_argc = argc;
 	data->m_args.m_argv = argv2;


	for (i=0;i<numThreads;i++)
	{
		data->m_threadSupport->runTask(B3_THREAD_SCHEDULE_TASK, (void*) &data->m_args, i);
	}

	return data;
}

bool btIsExampleBrowserTerminated(btInProcessExampleBrowserInternalData* data)
{
	return (data->m_args.m_cs->getSharedParam(0)==eExampleBrowserHasTerminated);
}

void btShutDownExampleBrowser(btInProcessExampleBrowserInternalData* data)
{
	int numActiveThreads = 1;

	data->m_args.m_cs->lock();
	data->m_args.m_cs->setSharedParam(0,eRequestTerminateExampleBrowser);
	data->m_args.m_cs->unlock();

	while (numActiveThreads)
                {
			int arg0,arg1;
                        if (data->m_threadSupport->isTaskCompleted(&arg0,&arg1,0))
                        {
                                numActiveThreads--;
                                printf("numActiveThreads = %d\n",numActiveThreads);

                        } else
                        {
//                              printf("polling..");
                        }
                };

	printf("stopping threads\n");
	delete data->m_threadSupport;
	delete data;
}

