#include "common.h"
#include "BoundedBuffer.h"
#include "Histogram.h"
#include "common.h"
#include "HistogramCollection.h"
#include "TCPreqchannel.h"

using namespace std;

struct replyPair {
    double reply = 0;
    int person = 0;
    bool done = false;
};

// patient thread function
// 1. creates (n) data messages from patient (p)
// 2. stores (n) data messages to BoundedBuffer
// 3. BoundedBuffer is passed by reference so it is updated in main
void patient_thread_function(BoundedBuffer* request, int totalPatients, int p, int e, int n) {

    // initialize time to 0;
    double t = 0;

    // create a datamsg
    datamsg message(p,t,e);

    // create and store n data messages to the request buffer
    for(int i = 0; i < n; i++) {

        // set time
        message.seconds = t;

        // create a buffer
        char* buffer;

        // convert messagePair to char* and assign it to the buffer
        buffer = (char*)&message;

        ////////////////////////////////////
        // datamsg* testPrint = (datamsg*)buffer;
        // int pp = testPrint->person;
        // double testECG = testPrint->seconds;

        // cout << "person: " << pp << " time: " << testECG << endl;
        ////////////////////////////////////

        // push the messagePair to the request buffer
        request->push(buffer, sizeof(datamsg)); // potential error with size here

        // increment time
        t += 0.004;
    }

}

// file request thread
void file_request_function(int m, bool file, string filename, TCPRequestChannel* chan, BoundedBuffer* request) {
    if (file) {
        // open file
        FILE *pFile;
        string rec_name = "recieved/" + filename;
        const char* name = rec_name.c_str();
        pFile = fopen(name, "wb");

        if (pFile == NULL) {
            cout << filename << " could not be created/opened." << endl;
        } else {
            // close file
            fclose(pFile);
        }

        string fname = filename;
        filemsg fm(0,0);
        int len = sizeof (filemsg) + fname.size()+1;
        char buf2[len];
        memcpy(buf2, &fm, sizeof(filemsg));
        strcpy(buf2 + sizeof(filemsg), fname.c_str());
        chan->cwrite(buf2,len);

        // get total file size
        __int64_t fileSize;
        int nbytes = chan->cread(&fileSize, sizeof(__int64_t));

        // initialize fm
        fm.offset = 0;
        fm.length = m; // this is a parameter passed by args

        // number of time to loop
        int finalLoop = (fileSize/m) + 1;

        int size = m;
        // loop for numRequests of correct size

        for (int i = 0; i < finalLoop; i++) {
            // update buffer to send to conatain new offset and length information
            int fileMsgLen = sizeof (filemsg) + fname.size()+1;
            char bufferSend [fileMsgLen];
            memcpy (bufferSend, &fm, sizeof(filemsg));
            strcpy (bufferSend + sizeof (filemsg), fname.c_str());

            // store bufferSend to request buffer
            request->push(bufferSend, fileMsgLen);

            // update fm offset
            fm.offset += size;

            // keep track of remaining bytes
            fileSize = fileSize - fm.length;
            fileSize = abs(fileSize);

            // if on the last loop
            if (i == (finalLoop - 2)) {
                // do a unique read and write
                // dynamically update the size
                size = fileSize;
                fm.length = size;
            }
        }

        // close channel once while loop breaks
        MESSAGE_TYPE message_Q = QUIT_MSG;
        chan->cwrite(&message_Q, sizeof (MESSAGE_TYPE));
    }
}



// worker thread function
// 1. pops request from the request buffer
// 2. takes note of patient number
// 3. sends request to server
// 4. deletes request off heap
// 5. collects response from server
// 6. creates STL::pair
// 7. stores patient number and response into pair
// 8. stores STL::pair into response buffer
// 9. keeps doing this while requests exist ---------> think about this more ... how many times should worker iterate?

// theres gonna be two methods for the worker
    // data request
    // file request

void worker_thread_function(BoundedBuffer* request, BoundedBuffer* response, TCPRequestChannel* chan, int bufcap, string filename, bool file) {
    string fname = "recieved/" + filename;
    while (true) {
        // check if file message or data message
        if (file) { // reqeust is a file reqeust

            // calculate fileMsgLen
            int fileMsgLen = sizeof (filemsg) + filename.size()+1;

            // pop from request buffer 
            char bufferSend [fileMsgLen];
            request->pop(bufferSend, fileMsgLen);

            // check to see if it is a quit message
            filemsg* message = (filemsg*)&bufferSend;
            if (message->mtype == QUIT_MSG) {
                // add it back to the request buffer for other workers to see
                char* quitBuffer = (char*)message;
                request->push(quitBuffer, sizeof(filemsg));
                // break
                break;
            }

            // get data from server (both message->lenth used to be size)
            chan->cwrite (bufferSend, fileMsgLen); // question
            char bufferRecieve[message->length]; 
            int nbytes = chan->cread(&bufferRecieve, message->length); // answer


            // open file
            FILE *pFile;
            pFile = fopen((char*)fname.c_str(), "r+b");

            // cout << "Chunk offset: " << message->offset << endl;

            // update position indicator
            fseek(pFile, message->offset, SEEK_SET);

            // write to file
            fwrite(&bufferRecieve, message->length, 1, pFile);

            // close file
            fclose(pFile);


        } else { // request is a data point request

            // create a buffer
            char buffer[bufcap];

            // pop request from request buffer into buffer
            int buff_bytes = request->pop(buffer, bufcap);

            // convert the buffer to datamsg and store person
            datamsg* message = (datamsg*)&buffer;
            int person = message->person;

            datamsg send = *message;

            if (send.mtype == QUIT_MSG) {
                // cout << "worker: quit message found !" << endl;
                // push the quti_msg back to response buffer for other threads to see
                char* quitBuffer = (char*)&send;
                request->push(quitBuffer, sizeof(datamsg));
                // break
                break;
            }

            // ////////////////////////////
            // double time = message->seconds;
            // cout << "PERSON: " << person << endl;
            // cout <<"TIME: " << time << endl;
            // ////////////////////////////

            // send message to server and get data
            chan->cwrite(&send, sizeof(datamsg)); // question
            double reply;
            int nbytes = chan->cread(&reply, sizeof(double)); // answer

            ////////////////////////
            // cout << "REPLY: " << reply << endl;
            ////////////////////////


            // create a response to send
            replyPair replyMessage;
            replyMessage.person = person;
            replyMessage.reply = reply;

            // cout << "Reply: " << reply << endl ;

            // convert response to a buffer
            char* sendbuffer = (char*)&replyMessage;

            // push sendbuffer
            response->push(sendbuffer, sizeof(replyPair));
        }
    }

    // close channel once while loop breaks
    MESSAGE_TYPE message_Q = QUIT_MSG;
    chan->cwrite(&message_Q, sizeof (MESSAGE_TYPE));
}
void histogram_thread_function (BoundedBuffer* response, int bufcap, HistogramCollection* histCol){
    while(true) {

        // create a buffer
        char buffer[bufcap];

        // pop from response and store in buffer
        int nbytes = response->pop(buffer, bufcap);

        // convert char* to datamsgPair
        replyPair* replyMessage = (replyPair*)&buffer;

        // check if it is a quit message
        bool done = replyMessage->done;

        // if it is a quit message, add the message back to the response buffer (for other threads to read) and break
        if (done == true) {
            char* buffer;
            replyPair quitMessage = *replyMessage;
            char* sendbuffer = (char*)&quitMessage;
            response->push(sendbuffer, sizeof(replyPair));
            break;
        } else { // regular message, add to hist
            // store patient number
            int p = replyMessage->person;

            // store data value
            double value = replyMessage->reply;

            // update hist corresponding to p
            histCol->update_hist(p-1, value);
        }

    }
}


int main(int argc, char *argv[])
{
    int opt;
    int n = 15000;    		//default number of requests per "patient"
    int p = 15;     		// number of patients [1,15]
    int w = 100;   		//default number of worker threads
    int b = 1024; 		    // default capacity of the request buffer, you should change this default
	int m = 256;       	// default capacity of the message buffer
    int e = 1;              // default ecg value for requesting data points
    int h = 50;            // number of hist threads
    string filename = "10MB";
    bool file = true;
    srand(time_t(NULL));



    // port number
    // string portNum = "5555";
    string portNum = "10777";

    // string ipAddress = "65.21.156.0";
    string ipAddress = "192.168.0.12";
    // string ipAddress = "127.0.0.1";

    // get arguments
    while ((opt = getopt(argc, argv, "n:p:w:b:m:e:h:f:a:r:")) != -1) {
        switch (opt) {
            case 'n':
                n = atoi(optarg);
                break;
            case 'p':
                p = atoi(optarg);
                break;
            case 'w':
                w = atoi(optarg);
            case 'b':
                b = atoi(optarg);
                break;
            case 'm':
                m = atoi(optarg);
                break;
            case 'e':
                e = atoi(optarg);
                break;
            case 'h':
                h = atoi(optarg);
                break;
            case 'f':
                filename = optarg;
                file = true;
                break;

            // ip address
            case 'a':
                ipAddress = optarg;
                break;

            // port number
            case 'r':
                portNum = atoi(optarg);
                break;
        }
    }
    
    // server is not ran with fork and exec
    // server should be ran on another machine
    // args for server: PORT NUMBER and BUFFER CAP
    
    // all constructors for client side TCPReuestChannels should include non-empty hostnames/ipAddress
    // there is no need for a control channel

    BoundedBuffer request_buffer(b);
    BoundedBuffer response_buffer(b);
	HistogramCollection hc;

    // create p number of histograms
    for (int i = 0; i < p; i++) {
        // create the hist
        Histogram* hist = new Histogram(10,-2,2);

        // push hist to collection
        hc.add(hist);
    } // empty histograms have been made for all the the patients
    
    struct timeval start, end;
    gettimeofday (&start, 0);

    // create a vector to hold each of the threads
    vector<thread> patientThreads;
    vector<thread> workerThreads;
    vector<thread> histThreads;
    vector<thread> fileThread;

    if (file) {
        // create and join a new channel
        TCPRequestChannel* newChan = new TCPRequestChannel(ipAddress, portNum);

        // open thread
        fileThread.push_back(thread(file_request_function, m, file, filename, newChan, &request_buffer));

    } else { // open patient request thread
        // loop creating (p) patient threads
        for (int i = 0; i < p; i ++) {
            // void patient_thread_function(BoundedBuffer& request, int p, int e, int n);
            patientThreads.push_back(thread(patient_thread_function, &request_buffer, p, (i+1), e, n)); 
            // cout << "create_PT(" << i << ")" << endl;
        }
    }

    //loop creating (w) worker threads
    for (int i = 0; i < w; i ++) {
        // create and join a new channel
        TCPRequestChannel* newChan = new TCPRequestChannel(ipAddress, portNum);

        // void worker_thread_function(BoundedBuffer& request, BoundedBuffer& response, TCPRequestChannel* chan, int bufcap, string filename, bool file);
        workerThreads.push_back(thread(worker_thread_function, &request_buffer, &response_buffer, newChan, b, filename, file));
        // cout << "create_WT(" << i << ")" << endl;
    }

    if (!file) {
        // create a for loop createing (h) hist threads
        for (int i = 0; i < h; i ++) {
            // void histogram_thread_function (BoundedBuffer& response, int bufcap, HistogramCollection& histCol)
            histThreads.push_back(thread(histogram_thread_function, &response_buffer, b, &hc));
            // cout << "create_HT(" << i << ")" << endl;
        }
    }

    //************************//
	/* Join all threads here */
    //***********************//

    if (file) {
        // join file request thread
        fileThread[0].join();
    } else {
        // cout << "joining patients" << endl;
        for (int i = 0; i < p; i++) {
            patientThreads[i].join();
            // cout << "join_PT(" << i << ")" << endl;
        }
    }

    //////////////////////////////////////////////////
    // send a quit message to request buffer

    if (file) {
        // send a quit message in form of a filesmg
        filemsg file_quitMessge(0,0);
        file_quitMessge.mtype = QUIT_MSG;
        char* file_sendbuffer = (char*)&file_quitMessge;
        request_buffer.push(file_sendbuffer, sizeof(filemsg));

    } else {
        // create a response to send
        datamsg req_quitMessage(0,0,0);
        req_quitMessage.mtype = QUIT_MSG;

        // convert response to a buffer
        char* req_sendbuffer = (char*)&req_quitMessage;

        // push sendbuffer
        request_buffer.push(req_sendbuffer, sizeof(datamsg));
    }
    //////////////////////////////////////////////////

    // cout << "joining workers" << endl;
    for (int i = 0; i < w; i++) {
        workerThreads[i].join();
        // cout << "join_WT(" << i << ")" << endl;
    }

    if (!file) { // close hist threads
        //////////////////////////////////////////////////
        // send a quit message to response buffer
        // create a response to send
        replyPair res_quitMessage;
        res_quitMessage.done = true;

        // convert response to a buffer
        char* res_sendbuffer = (char*)&res_quitMessage;

        // push sendbuffer
        response_buffer.push(res_sendbuffer, sizeof(replyPair));
        //////////////////////////////////////////////////

        // cout << "joining hists" << endl;
        for (int i = 0; i < h; i++) {
            histThreads[i].join();
            // cout << "join_HT(" << i << ")" << endl;
        }

    }


    gettimeofday (&end, 0);

    if (file) {
        cout << "File created." << endl;
    } else {
        // print the results
	    hc.print ();
    }

    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)/(int) 1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec)%((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;
    
}