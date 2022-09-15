/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name:Andrew Lackey
	UIN:828008194
	Date:2/10/2022
*/
#include <sys/wait.h>
#include "common.h"
#include "FIFORequestChannel.h"

using namespace std;


int main (int argc, char *argv[]) {
	clock_t start, stop;
	start = clock(); //my timing stuff

	int opt;
	int p = -1;
	double t = -1;
	int e = -1;
	bool new_channel = false;
	char* new_buffer;
	int buffer_size = MAX_MESSAGE; //default 256

	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'c':
				new_channel = true;
				break;
			case 'm':
				buffer_size = atoi (optarg);
				new_buffer = optarg;
				break;
		}
	}

	//runs server as child of client
	pid_t pid = fork();
	if(!pid){
		if(buffer_size != 256){//adjust buffer size if needed on server side
			char* args[] = {(char*) string("./server").c_str(), (char*) string("-m").c_str(), new_buffer, NULL};
			execvp(args[0], args);
			perror("exec failing");
		}else{//buffer size 256 (default)
			char* args[2] = {(char*) string("./server").c_str(), NULL};
			execvp(args[0], args);
			perror("exec failing");
		}
	}


	string channel_name = "control";

	FIFORequestChannel * control = new FIFORequestChannel(channel_name, FIFORequestChannel::CLIENT_SIDE);

    FIFORequestChannel * chan = control;

	//switch channels if needed
	if(new_channel){
		MESSAGE_TYPE new_chan = NEWCHANNEL_MSG;
		control->cwrite(&new_chan, sizeof(MESSAGE_TYPE));
		char * buf_newc = new char[buffer_size];
		control->cread(buf_newc, sizeof(string));
		channel_name = buf_newc;
		delete[] buf_newc;
		chan = new FIFORequestChannel(channel_name.c_str(), FIFORequestChannel::CLIENT_SIDE);
		//cout << "control:" << control << endl;
		//cout << "chan:" << chan << endl;
	}

	char buf[MAX_MESSAGE]; // 256
	//we are reading 1000 lines of data
	if(p != -1 && t == -1 && e == -1 && filename == ""){
		int entry_number = 0;
		ofstream x1File("received/x1.csv");
		for(double i = 0; i < 4; i += .004){
			entry_number += 1;
			datamsg ecg1(p, i, 1);
			memcpy(buf, &ecg1, sizeof(datamsg));
			chan->cwrite(buf, sizeof(datamsg)); // question
			double reply1;
			chan->cread(&reply1, sizeof(double)); //answer
			datamsg ecg2(p, i, 2);
			memcpy(buf, &ecg2, sizeof(datamsg));
			chan->cwrite(buf, sizeof(datamsg)); // question
			double reply2;
			chan->cread(&reply2, sizeof(double)); //answer
			if(entry_number == 1000){
				x1File << i << "," <<  reply1 << "," << reply2;
			}else{
				x1File << i << "," << reply1 << "," << reply2 << "\n";
			}
		}
		x1File.close();
	}
	//we are reading a single data point
	else if(p != -1 && t != -1 && e != -1 && filename == ""){
	
		datamsg x(p, t, e);

		memcpy(buf, &x, sizeof(datamsg));
		chan->cwrite(buf, sizeof(datamsg)); // question
		double reply;
		chan->cread(&reply, sizeof(double)); //answer
		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	}
	//we are transfering a file
	else if(p == -1 && t == -1 && e == -1 && filename != ""){

		//cout << "file buffer size :" << buffer_size << endl;
		filemsg fm(0, 0);
		string fname = filename; //added filename

		int len = sizeof(filemsg) + (fname.size() + 1);
		char* buf2 = new char[len];
		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), fname.c_str());
		chan->cwrite(buf2, len);  // I want the file length;
		__int64_t size; //size of file
		chan->cread(&size, sizeof(__int64_t));
		int iterations = ceil(double(size)/buffer_size); //calculates above iterations

		filemsg* fm2 = (filemsg*) buf2;
		fm2->offset = 0;//starting offset

		if(iterations == 1){ //does it take 1 cycle
			fm2->length = size;

		}else{ //doesnt take 1 cycle
			fm2->length = buffer_size;
		}

		__int64_t last_size = size - buffer_size * (iterations - 1); //final cycle size
		chan->cwrite(buf2, len);
		char* buf3 = new char[buffer_size];
		chan->cread(buf3, buffer_size);
		FILE* fp = fopen(("received/"+fname).c_str(), "wb");
		fwrite(buf3, 1, fm2->length, fp);
		for(int i = 1; i < iterations; i++){

			if(i == iterations-1){ //last cycle
				//cout << iterations << " " << i << " " << last_size << " Jump" << endl;
				fm2->length = last_size;
				delete[] buf3;
				buf3 = new char[last_size];
				fm2->offset += buffer_size;
				chan->cwrite(buf2, len);
				chan->cread(buf3, buffer_size);
				fwrite(buf3, 1, fm2->length, fp);

			}else{ //every other cycle
				fm2->offset += buffer_size;
				chan->cwrite(buf2, len);
				chan->cread(buf3, buffer_size);
				fwrite(buf3, 1, fm2->length, fp);
			}
		}
		fclose(fp);
		delete[] buf3;
		delete[] buf2;

	}

    // sending a non-sense message, you need to change this
	stop = clock();
	double time_taken = double(stop - start) / double(CLOCKS_PER_SEC);
	cout << "time taken: " << fixed << time_taken << endl;
	// closing the channel    
    MESSAGE_TYPE m = QUIT_MSG;

	if(new_channel){//if we have new channel send quit
		chan->cwrite(&m, sizeof(MESSAGE_TYPE));
		delete chan;
	}//send quit to control no matter what
	control->cwrite(&m, sizeof(MESSAGE_TYPE));
	delete control;
	wait(NULL);
	cout << "client exited" << endl;
}
