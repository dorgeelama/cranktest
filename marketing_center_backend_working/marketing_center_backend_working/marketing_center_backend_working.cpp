#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <iostream>
#include <vector>
#include<string.h>
#include <atlstr.h>
#include <sstream>
#include<cstdio>
#include<WinBase.h>
#include<minwinbase.h>
#include<windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define maxBytes 111
#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h> // for usleep
#endif
#include <gre/greio.h>
#include "marketing_center_events.h"
#include <string>
#define MARKETING_SEND_CHANNEL "marketing_center_frontend"
#define MARKETING_RECEIVE_CHANNEL "marketing_center_backend"
#define SNOOZE_TIME 80
static int							dataChanged = 1; //Default to 1 so we send data to the ui once it connects
static dispenser_is_empty_event_t card_dispenser_state;
static link_test_working_event_t link_test_data;
#ifdef WIN32
static CRITICAL_SECTION lock;
static HANDLE thread1;
#else 
static pthread_mutex_t lock;
static pthread_t 	thread1;
#endif
#pragma warning(disable:4996) 
// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

/**
*Message format is stx command/response data etx bcc
* stx 02
* dle 10
* etx 03
* enq 05
* ack 06
* nak 15
*/
char dispense_command[5] = { 0x02,0x43,0x44,0x03,0x04 };
char status_request[5] = { 0x02,0x53,0x52,0x03,0x02 };
char enquire_command[1] = { 0x05 };
bool dispenser_empty = false;
bool dispense_card = false;
bool link_status = false;
/**
 * cross-platform function to create threads
 * @param start_routine This is the function pointer for the thread to run
 * @return 0 on success, otherwise an integer above 1
 */
int
create_task(void* start_routine) {
#ifdef WIN32
	thread1 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)start_routine, NULL, 0, NULL);
	if (thread1 == NULL) {
		return 1;
	}
	return 0;
#else
	return pthread_create(&thread1, NULL, start_routine, NULL);
#endif
}

/**
 * cross platform mutex initialization
 * @return 0 on success, otherwise an integer above 1
 */
int
init_mutex() {
#ifdef WIN32
	InitializeCriticalSection(&lock);
	return 0;
#else
	return pthread_mutex_init(&lock, NULL);
#endif
}

/**
 * cross platform mutex lock
 */
void
lock_mutex() {
#ifdef WIN32
	EnterCriticalSection(&lock);
#else
	pthread_mutex_lock(&lock);
#endif
}

HANDLE hSerial, linkSerial;
DCB dcbSerialParams = { 0 };
COMMTIMEOUTS timeouts = { 0 };
BOOL Status, linkStatus;
unsigned char szBuff[maxBytes] = { 0 };
unsigned char szBuffLinkSerial[maxBytes] = { 0 };
DWORD dwBytesRead, dwBytesReadLink = 0;
HANDLE hPrinter = INVALID_HANDLE_VALUE;
PRINTER_INFO_5* printer = nullptr;
DWORD numOfBytes = 0;
DWORD numOfStructs = 0;
bool failOrSucceed = EnumPrinters(PRINTER_ENUM_LOCAL, NULL, 5, NULL, NULL, &numOfBytes, &numOfStructs);
std::unique_ptr < BYTE[]>ptrBuffer(new BYTE[numOfBytes]);
char print_bit[]{ 0x01C, 0x70, 0x01, 0x00 };
DOC_INFO_1 doc;
LPDWORD bytes_w = 0;
char cut[2] = { 0x1B, 0x69 };
/**
 * cross platform mutex unlock
 */
void
unlock_mutex() {
#ifdef WIN32
	LeaveCriticalSection(&lock);
#else
	pthread_mutex_unlock(&lock);
#endif
}

/**
 * cross-platform sleep
 */
void
sleep_ms(int milliseconds) {
#ifdef WIN32
	Sleep(milliseconds);
#else
	usleep(milliseconds * 1000);
#endif
}


void write_comport(char* packet, USHORT Size)
{
	hSerial = CreateFile(L"COM3", GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (hSerial == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() == ERROR_FILE_NOT_FOUND)
		{
			printf("Serial port does not exist\n");
		}
		printf("Other errors\n");
	}

	printf("\n\n +==========================================+");
	printf("\n |    Serial Port  Reception (Win32 API)    |");
	printf("\n +==========================================+\n");

	//setting parameters
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

	//GetCommState is to retrieves the current control settings for a specific communications device.
	if (!GetCommState(hSerial, &dcbSerialParams))
	{
		printf("Not GetCommState, not able to retrieves the current control\n");
	}

	dcbSerialParams.BaudRate = CBR_9600;
	dcbSerialParams.ByteSize = 8;
	dcbSerialParams.StopBits = ONESTOPBIT;
	dcbSerialParams.Parity = NOPARITY;

	Status = SetCommState(hSerial, &dcbSerialParams);  //Configuring the port according to settings in DCB

	if (Status == FALSE)
	{
		printf("\n    Error! in Setting DCB Structure");
	}
	else //If Successfull display the contents of the DCB Structure
	{
		printf("\n       Setting DCB Structure Successful\n");
		printf("\n       Baudrate = %d", dcbSerialParams.BaudRate);
		printf("\n       ByteSize = %d", dcbSerialParams.ByteSize);
		printf("\n       StopBits = %d", dcbSerialParams.StopBits);
		printf("\n       Parity   = %d\n", dcbSerialParams.Parity);
	}

	//SetCommState configures a communications device according to the specifications
	//in DCB. The function reinitializes all hardware control settings but it does not
	//empty output or input queues
	if (!SetCommState(hSerial, &dcbSerialParams))
	{
		printf("Not SetCommState, cannot configures serial port according to DCB specifications set\n");
	}

	//setting timeouts
	timeouts.ReadIntervalTimeout = 40;
	timeouts.ReadTotalTimeoutConstant = 40;
	timeouts.ReadTotalTimeoutMultiplier = 40;
	timeouts.WriteTotalTimeoutConstant = 40;
	timeouts.WriteTotalTimeoutMultiplier = 40;

	//SetCommTimeouts set the time out parameters for all read and write operation
	if (!SetCommTimeouts(hSerial, &timeouts))
	{
		printf("Not SetCommTimeouts, cannot set the timeout parameters to serial port\n");
	}
	DWORD dNoOfBytesWritten = 0;     // No of bytes written to the port
	Status = WriteFile(hSerial,        // Handle to the Serial port
		packet,     // Data to be written to the port
		Size,  //No of bytes to write
		&dNoOfBytesWritten, //Bytes written
		NULL);
	if (Status == 1) {
		std::cout << "Data successfully written to the comport Status=" << Status << std::endl;
	}
	else {
		std::cout << "could not write to comport" << std::endl;
	}
	CloseHandle(hSerial);
}






/**
* enquire about the status of the card dispenser
* Whether it is empty or not
* stuck or not
* etc...
* */

void card_dispenser_status()
{
	hSerial = CreateFile(L"COM3", GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (hSerial == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() == ERROR_FILE_NOT_FOUND)
		{
			printf("Serial port does not exist\n");
		}
		printf("Other errors\n");
	}

	printf("\n\n +==========================================+");
	printf("\n |    Serial Port  Reception (Win32 API)    |");
	printf("\n +==========================================+\n");

	//setting parameters
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

	//GetCommState is to retrieves the current control settings for a specific communications device.
	if (!GetCommState(hSerial, &dcbSerialParams))
	{
		printf("Not GetCommState, not able to retrieves the current control\n");
	}

	dcbSerialParams.BaudRate = CBR_9600;
	dcbSerialParams.ByteSize = 8;
	dcbSerialParams.StopBits = ONESTOPBIT;
	dcbSerialParams.Parity = NOPARITY;

	Status = SetCommState(hSerial, &dcbSerialParams);  //Configuring the port according to settings in DCB

	if (Status == FALSE)
	{
		printf("\n    Error! in Setting DCB Structure");
	}
	else //If Successfull display the contents of the DCB Structure
	{
		printf("\n       Setting DCB Structure Successful\n");
		printf("\n       Baudrate = %d", dcbSerialParams.BaudRate);
		printf("\n       ByteSize = %d", dcbSerialParams.ByteSize);
		printf("\n       StopBits = %d", dcbSerialParams.StopBits);
		printf("\n       Parity   = %d\n", dcbSerialParams.Parity);
	}

	//SetCommState configures a communications device according to the specifications
	//in DCB. The function reinitializes all hardware control settings but it does not
	//empty output or input queues
	if (!SetCommState(hSerial, &dcbSerialParams))
	{
		printf("Not SetCommState, cannot configures serial port according to DCB specifications set\n");
	}

	//setting timeouts
	timeouts.ReadIntervalTimeout = 40;
	timeouts.ReadTotalTimeoutConstant = 40;
	timeouts.ReadTotalTimeoutMultiplier = 40;
	timeouts.WriteTotalTimeoutConstant = 40;
	timeouts.WriteTotalTimeoutMultiplier = 40;

	//SetCommTimeouts set the time out parameters for all read and write operation
	if (!SetCommTimeouts(hSerial, &timeouts))
	{
		printf("Not SetCommTimeouts, cannot set the timeout parameters to serial port\n");
	}


	DWORD dNoOfBytesWritten = 0;     // No of bytes written to the port

	Status = WriteFile(hSerial,        // Handle to the Serial port
		enquire_command,     // Data to be written to the port
		1,  //No of bytes to write
		&dNoOfBytesWritten, //Bytes written
		NULL);
	if (Status == 1) {
		std::cout << "Data successfully written to the comport Status=" << Status << std::endl;
	}
	else {
		std::cout << "could not write to comport" << std::endl;
	}

	//reading data
	//ReadFile reads data from the specified file or i/o devices.
	if ((ReadFile(hSerial, szBuff, maxBytes, &dwBytesRead, NULL) && szBuff[0] == 'R')) {
		std::cout << "Card dispenser is ready" << std::endl;
		std::cout << "Sending Status Request " << std::endl;
		Status = WriteFile(hSerial,        // Handle to the Serial port
			status_request,     // Data to be written to the port
			5,  //No of bytes to write
			&dNoOfBytesWritten, //Bytes written
			NULL);
		ReadFile(hSerial, szBuff, maxBytes, &dwBytesRead, NULL);
		std::cout << "Status Request Response: " << szBuff << std::endl;
		if (szBuff[5] == '0') {
			std::cout << "no card in unit" << std::endl;
			dispenser_empty = true;
		}
		else if (szBuff[3] == '0' && szBuff[4] == '0' && szBuff[5] == '1' && szBuff[6] == '0' && szBuff[7] == '0') {
			dispense_card = true;
			std::cout << "Dispense card =  " << dispense_card << std::endl;
		}
	}
	CloseHandle(hSerial);
}







/**
 * Definition for the receive thread
 */
void*
receive_thread(void* arg) {



	gre_io_t* handle;
	gre_io_serialized_data_t* nbuffer = NULL;
	char* event_addr;
	char* event_name;
	char* event_format;
	void* event_data;
	int						 ret;
	int nbytes;

	printf("Opening a channel for receive\n");
	// Connect to a channel to receive messages
	handle = gre_io_open(MARKETING_RECEIVE_CHANNEL, GRE_IO_TYPE_RDONLY);
	if (handle == NULL) {
		fprintf(stderr, "Can't open receive channel\n");
		return 0;
	}

	nbuffer = gre_io_size_buffer(NULL, 100);

	while (1) {
		ret = gre_io_receive(handle, &nbuffer);
		if (ret < 0) {
			return 0;
		}

		event_name = NULL;
		nbytes = gre_io_unserialize(nbuffer, &event_addr, &event_name, &event_format, &event_data);
		if (!event_name) {
			printf("Missing event name\n");
			return 0;
		}

		printf("Received Event %s nbytes: %d format: %s\n", event_name, nbytes, event_format);

		lock_mutex();
		if (strcmp(event_name, DISPENSER_STATUS_EVENT) == 0) {
			dispenser_status_event_t* uidata = (dispenser_status_event_t*)event_data;
			int status = uidata->status;
			if (status == 1) {
				card_dispenser_status();
			}
			dataChanged = 1;
		}
		else if (strcmp(event_name, DISPENSE_CARD_EVENT) == 0) {
			dispense_card_event_t* uidata = (dispense_card_event_t*)event_data;
			int test = uidata->card;
			std::cout << "Dispensing card" << std::endl;
			std::cout << "test" << test << std::endl;
			if (test == 1 && dispense_card) {
				std::cout << "before writing to comport" << std::endl;
				write_comport(dispense_command, 5);
				std::cout << "line 386 dispense command sent" << std::endl;
			}
			dataChanged = 1;
		}
		else if (strcmp(event_name, LINK_TEST_EVENT) == 0) {
			link_test_event_t* uidata = (link_test_event_t*)event_data;
			int test = uidata->l;
			if (test == 1) {
				link_status = true;
				std::cout << "Link test initial" << std::endl;
			}
			dataChanged = 1;
		}
		else if (strcmp(event_name, PRINT_RECEIPT_EVENT) == 0) {
			failOrSucceed = EnumPrinters(PRINTER_ENUM_LOCAL, NULL, 5, ptrBuffer.get(), numOfBytes, &numOfBytes, &numOfStructs);
			printer = (PRINTER_INFO_5*)ptrBuffer.get();
			std::vector<LPWSTR> printerList;
			for (UINT i = 0; i < numOfStructs; i++) {
				printerList.push_back(printer->pPrinterName);
				printer++;
			}

			DWORD dwJob;
			bool openPrinter = OpenPrinter(printerList[4], &hPrinter, NULL);
			std::cout << "printer opened: " << openPrinter << std::endl;
			if (!openPrinter) {
				std::cout << "Error opening printer!" << std::endl;
			}
			doc.pDocName = (LPTSTR)L"My Document";
			doc.pDatatype = (LPTSTR)L"RAW";
			doc.pOutputFile = NULL;
			StartDocPrinter(hPrinter, 1, (LPBYTE)&doc);
			StartPagePrinter(hPrinter);
			char printThistext[] = "\n          CAR WASH\n   10-oct-2022      12:59 PM        \n purchase Txn:0:1349 \n Gold Wash             $20.00\n  \n \n \n \n \n ";
			print_receipt_event_t* uidata = (print_receipt_event_t*)event_data;
			int print = uidata->receipt;
			std::cout << "Printing Receipt" << std::endl;
			if (print == 1) {
				bool write_to_printer = WritePrinter(hPrinter, print_bit, sizeof(print_bit), (LPDWORD)&bytes_w);
				WritePrinter(hPrinter, printThistext, strlen(printThistext), (LPDWORD)&bytes_w);
				WritePrinter(hPrinter, cut, sizeof(cut), (LPDWORD)&bytes_w);

				//	std::cout << "Writing to printer" << write_to_printer << std::endl; EndPagePrinter(hPrinter);
				EndDocPrinter(hPrinter);
				ClosePrinter(hPrinter);
				std::cout << "this is after the printer is closed" << std::endl;
			}
			dataChanged = 1;
		}
		unlock_mutex();
	}

	//Release the buffer memory, close the send handle
	gre_io_free_buffer(nbuffer);
	gre_io_close(handle);
}

void link_test(gre_io_t* send_handle) {
	std::cout << "before the reading link data" << std::endl;
	bool read_link = ReadFile(linkSerial, szBuffLinkSerial, maxBytes, &dwBytesReadLink, NULL);
	if (read_link) {
		std::cout << "read data for link " << std::endl;
		std::cout << "szBuff Link" << szBuff[0] << szBuff[1] << std::endl;

		}
	std::cout << "after reading link data" << std::endl;
	gre_io_serialized_data_t* nbuffer = NULL;
	link_test_working_event_t event_data;
	event_data.link = 1;
	event_data.link_data = 123456;
	int ret;

	nbuffer = gre_io_serialize(nbuffer, NULL, LINK_TEST_WORKING_EVENT, LINK_TEST_WORKING_FMT, &event_data, sizeof(event_data));
	if (!nbuffer) {
		fprintf(stderr, "Can't serialized data to buffer, exiting\n");
		return;
	}

	// Send the serialized event buffer
	ret = gre_io_send(send_handle, nbuffer);
	if (ret < 0) {
		fprintf(stderr, "Send failed, exiting\n");
	}
	//Release the buffer memory
	gre_io_free_buffer(nbuffer);

}



int
main(int argc, char** argv) {




	DWORD dNoOfBytesWritten;
	gre_io_t* send_handle;
	gre_io_serialized_data_t* nbuffer = NULL;
	dispenser_is_empty_event_t event_data;
	int ret;
	time_t timer = time(NULL);
	double seconds;

	//allocate memory for the thermostat state
	memset(&card_dispenser_state, 0, sizeof(card_dispenser_state));
	//set initial state of the demo

	if (init_mutex() != 0) {
		fprintf(stderr, "Mutex init failed\n");
		return 0;
	}

	printf("Trying to open the connection to the frontend\n");
	while (1) {
		// Connect to a channel to send messages (write)
		sleep_ms(SNOOZE_TIME);
		send_handle = gre_io_open(MARKETING_SEND_CHANNEL, GRE_IO_TYPE_WRONLY);
		if (send_handle != NULL) {
			printf("Send channel: %s successfully opened\n", MARKETING_SEND_CHANNEL);
			break;
		}
	}

	if (create_task(receive_thread) != 0) {
		fprintf(stderr, "Thread create failed\n");
		return 0;
	}

	memset(&event_data, 0, sizeof(event_data));

	while (1) {
		sleep_ms(SNOOZE_TIME);
		if (dispenser_empty) {
			lock_mutex();
			card_dispenser_state.emptyOrNot = 1;
			event_data = card_dispenser_state;
			unlock_mutex();

			nbuffer = gre_io_serialize(nbuffer, NULL, DISPENSER_IS_EMPTY_EVENT, DISPENSER_IS_EMPTY_FMT, &event_data, sizeof(event_data));
			if (!nbuffer) {
				fprintf(stderr, "Can't serialized data to buffer, exiting\n");
				break;
			}

			// Send the serialized event buffer
			ret = gre_io_send(send_handle, nbuffer);
			if (ret < 0) {
				fprintf(stderr, "Send failed, exiting\n");
				break;
			}
			dispenser_empty = false;

		}
		else if (link_status) {
			link_test(send_handle);
			link_status = false;

		}



	}


	//Release the buffer memory, close the send handle
	gre_io_free_buffer(nbuffer);
	gre_io_close(send_handle);


	return 0;
}
