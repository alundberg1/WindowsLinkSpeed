#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <wchar.h>
#include <ifdef.h>
#include <wininet.h>
#include <ctime>
#include <fstream>
#include <stdio.h>
#include <direct.h>
#include <string.h>
//#include <iostream>

// Link with Iphlpapi.lib
#pragma comment(lib, "IPHLPAPI.lib")
#define WORKING_BUFFER_SIZE 15000
#define MAX_TRIES 3


#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
/* Note: could also use malloc() and free() */

#define INFO_BUFFER_SIZE 1000

/*
* Function for testing the sleep Function
* using a MessageBox
* Technically no longer needed
*/
void checkSleep() {
    MessageBox(0, L"Sleepy", L"Sleep", 0);
    Sleep(10000);
}

/*
* Function for grabbing the IP address of the computer running the program
* Uses GetAdaptersInfonand filters by Ethernet to pull the IP address
* Only called if there is an active internet connection through ethernet
* Returns a string containing the IP address
*/

std::wstring getIPAddress() {
    PIP_ADAPTER_INFO pAdapterInfo;
    PIP_ADAPTER_INFO pAdapter = NULL;
    DWORD dwRetVal = 0;
    std::string tmp;
    //UINT i;

    /* variables used to print DHCP time info */
    //struct tm newtime;
    //char buffer[32];
    //errno_t error;

    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
    pAdapterInfo = (IP_ADAPTER_INFO*)MALLOC(sizeof(IP_ADAPTER_INFO));
    if (pAdapterInfo == NULL) {
        printf("Error allocating memory needed to call GetAdaptersinfo\n");
    }
    // Make an initial call to GetAdaptersInfo to get
    // the necessary size into the ulOutBufLen variable
    if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
        FREE(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO*)MALLOC(ulOutBufLen);
        if (pAdapterInfo == NULL) {
            printf("Error allocating memory needed to call GetAdaptersinfo\n");
        }
    }
    //Iterate through internet adapters until ethernet is found
    //returns IP address once found
    if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR) {
        pAdapter = pAdapterInfo;
        while (pAdapter) {
            switch (pAdapter->Type) {
            case MIB_IF_TYPE_ETHERNET:
                tmp = pAdapter->IpAddressList.IpAddress.String;
                return std::wstring(tmp.begin(), tmp.end());
                break;
            default:
                break;
            }

            //Iterated to next adapter
            pAdapter = pAdapter->Next;
        }
    }
    else {
        printf("GetAdaptersInfo failed with error: %d\n", dwRetVal);

    }
    if (pAdapterInfo)
        FREE(pAdapterInfo);
    //If not Ethernet adapter found, return that it wasn't found
    //Code should never get here
    return L"Not found";
}

/*
* Function to check for an active internet connection
* Without this, internet would be considered good with no internet connection
* Uses IPForward table to check for Default route of 0.0.0.0
* If found, there is an active internet connection
* Returns a boolean, true if active internet connection, false otherwise
*/

bool IsInternetAvailable()
{
    bool bIsInternetAvailable = false;
    // Get the required buffer size
    DWORD dwBufferSize = 0;
    if (ERROR_INSUFFICIENT_BUFFER == GetIpForwardTable(NULL, &dwBufferSize, false))
    {
        BYTE *pByte = new BYTE[dwBufferSize];
        if (pByte)
        {
            PMIB_IPFORWARDTABLE pRoutingTable = reinterpret_cast<PMIB_IPFORWARDTABLE>(pByte);
            // Attempt to fill buffer with routing table information
            if (NO_ERROR == GetIpForwardTable(pRoutingTable, &dwBufferSize, false))
            {
                DWORD dwRowCount = pRoutingTable->dwNumEntries; // Get row count
                // Look for default route to gateway
                for (DWORD dwIndex = 0; dwIndex < dwRowCount; ++dwIndex)
                {
                    if (pRoutingTable->table[dwIndex].dwForwardDest == 0)
                    {                                // Default route designated by 0.0.0.0 in table
                        bIsInternetAvailable = true; // Found it
                        break;                       // Short circuit loop
                    }
                }
            }
            delete[] pByte; // Clean up. Just say "No" to memory leaks
        }
    }
    return bIsInternetAvailable;
}

/*
* Function to hide window
* Makes the program run in the background without command prompt
*/

void HideConsole()
{
    ::ShowWindow(::GetConsoleWindow(), SW_HIDE);
}

/*
* Function to show window
* Adds ability to show console
*/

void ShowConsole()
{
    ::ShowWindow(::GetConsoleWindow(), SW_SHOW);
}

/*
* Function to check window visivility
* Adds ability to check if window is shown
*/

bool IsConsoleVisible()
{
    return ::IsWindowVisible(::GetConsoleWindow()) != FALSE;
}

/*
* Function to check the link speed on an ethernet connection
* Uses GetAdapterAddresses to pull information on the adapter
* Link Speed pulled from TransmitLinkSpeed variable of PIP_ADAPTER_ADDRESSES struct
* Stores the current link speed in Mbps as Unsigned long long that was passed as parameter
* Returns false if link speed is fine, true if bad
*/

bool __cdecl checkSpeed(ULONG64 *speed)
{

    /* Declare and initialize variables */

    DWORD dwSize = 0;
    DWORD dwRetVal = 0;
    bool lowLinkSpeed = true;

    unsigned int i = 0;

    // Set the flags to pass to GetAdaptersAddresses
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;

    // default to unspecified address family (both)
    ULONG family = AF_UNSPEC;

    LPVOID lpMsgBuf = NULL;

    PIP_ADAPTER_ADDRESSES_LH pAddresses = NULL;
    ULONG outBufLen = 0;
    ULONG Iterations = 0;

    PIP_ADAPTER_ADDRESSES_LH pCurrAddresses = NULL;
    PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
    PIP_ADAPTER_ANYCAST_ADDRESS pAnycast = NULL;
    PIP_ADAPTER_MULTICAST_ADDRESS pMulticast = NULL;
    IP_ADAPTER_DNS_SERVER_ADDRESS *pDnServer = NULL;
    IP_ADAPTER_PREFIX *pPrefix = NULL;

    family = AF_INET;

    // Allocate a 15 KB buffer to start with.
    outBufLen = WORKING_BUFFER_SIZE;

    do
    {

        pAddresses = (IP_ADAPTER_ADDRESSES_LH *)MALLOC(outBufLen);
        if (pAddresses == NULL)
        {
            printf("Memory allocation failed for IP_ADAPTER_ADDRESSES struct\n");
            exit(1);
        }

        dwRetVal =
            GetAdaptersAddresses(family, flags, NULL, (PIP_ADAPTER_ADDRESSES)pAddresses, &outBufLen);

        if (dwRetVal == ERROR_BUFFER_OVERFLOW)
        {
            FREE(pAddresses);
            pAddresses = NULL;
        }
        else
        {
            break;
        }

        Iterations++;

    } while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));

    if (dwRetVal == NO_ERROR)
    {
        // If successful, output some information from the data we received
        pCurrAddresses = pAddresses;
        while (pCurrAddresses)
        {
            wchar_t *ws = (pCurrAddresses->FriendlyName);
            wchar_t *pwc;
            pwc = wcsstr(ws, L"Ethernet");
            if (pwc != NULL)
            {
                *speed = (pCurrAddresses->TransmitLinkSpeed) / 1000000;
                if (*speed >= 1000)
                {
                    //printf("\tLink speed fine \n ");
                    lowLinkSpeed = false;
                }
                else
                {
                    //printf("\tLink Speed Below 1000 Mbps, instead it is: %llu Mbps\n", speed);
                }
            }

            pCurrAddresses = pCurrAddresses->Next;
        }
    }
    else
    {
        printf("Call to GetAdaptersAddresses failed with error: %d\n",
               dwRetVal);
        if (dwRetVal == ERROR_NO_DATA)
            printf("\tNo addresses were found for the requested parameters\n");
        else
        {

            if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                  FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                              NULL, dwRetVal, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                              // Default language
                              (LPTSTR)&lpMsgBuf, 0, NULL))
            {
                //printf("\tError: %s", lpMsgBuf);
                LocalFree(lpMsgBuf);
                if (pAddresses)
                    FREE(pAddresses);
                exit(1);
            }
        }
    }

    if (pAddresses)
    {
        FREE(pAddresses);
    }
    return lowLinkSpeed;
}

int main(int argc, char * argv[])
{
    //hide command prompt
    HideConsole();
    std::string filePath = "";
    char AF[] = "-AF";
    // pull filepath from command line to store info on computer internet speed
    if (strcmp(argv[1], AF) == 0) {
        filePath += argv[2];
        printf("Path: %s\n", argv[2]);
    }
    bool linkSpeedPrevLow = false;
    bool done = false;
    bool lowLinkSpeed;
    bool filePathCreated = false;
    // infinite loop to run continuously
    while (!done)
    {
        // if internet connection available
        if (IsInternetAvailable())
        {
            ULONG64 speed;
            // pull link speed amount and determine if low
            lowLinkSpeed = checkSpeed(&speed);
            // if speed is low and was previously fine
            if (lowLinkSpeed && !linkSpeedPrevLow)
            {
                time_t rawtime;
                struct tm* timeInfo;
                char buffer[80];
                // create time stamp
                time(&rawtime);
                timeInfo = localtime(&rawtime);
                strftime(buffer, sizeof(buffer), "%m-%d-%Y %H:%M", timeInfo);
                std::string strStr(buffer);
                std::wstring str(strStr.begin(), strStr.end());
                std::string timeStr((char*)buffer);
                //if filePath was given in command line
                if (filePath != "") {
                    TCHAR  compName[INFO_BUFFER_SIZE];
                    TCHAR  userName[INFO_BUFFER_SIZE];
                    DWORD  bufCharCountComp = INFO_BUFFER_SIZE;
                    DWORD  bufCharCountUser = INFO_BUFFER_SIZE;
                    //Get name of the computer
                    GetComputerName(compName, &bufCharCountComp);
                    // Get current user
                    GetUserName(userName, &bufCharCountUser);
                    // If file name not previously appended to file path given,
                    // Add file name to be stored in directory
                    /*
                    if (!filePathCreated) {
                        std::wstring computerNameW(compName);
                        std::string computerName(computerNameW.begin(), computerNameW.end());
                        filePath += "\\" + (std::string)computerName + ".txt";
                        filePathCreated = true;
                    }
                    */
                    // Get ip address that computer is currently using
                    std::wstring IP = getIPAddress();
                    // Open file to write to
                    std::wofstream myFile(filePath);
                    // write Computer name, user, IP, Link speed, and time stamp to file
                    myFile << L"Computer Name: " << compName << L", User: " << userName << L", IP Address: " << IP << L", Link Speed: " << speed << L", Time: " << str << "\n";
                    //close file
                    myFile.close();
                    // create message to alert user of low internet speed
                    std::wstring lowSpeedMessageString = L"Low internet speed detected \n" + str;
                    LPCWSTR lowSpeedMessage = lowSpeedMessageString.c_str();
                    // alert user about low speed
                    MessageBox(0, lowSpeedMessage, L"LinkSpeed", 0);
                    linkSpeedPrevLow = true;
                }  
            }
            // if speed is not low and was not previously fine
            else
            {
                // if internet speed was previously low but is now fine
                if (linkSpeedPrevLow && !lowLinkSpeed)
                {
                    // IF filePath given in command line, delete the previouslt created file
                    if (filePath != "") {
                        const char* fP = filePath.c_str();
                        DeleteFileA(fP);
                    }
                    //Alert user that internet speed improved
                    MessageBox(0, L"Your internet speed is no longer low", L"Normal Internet Speed", 0);
                    linkSpeedPrevLow = false;
                    //done = true;
                }
            }
            //wait 10 min
            Sleep(600000);
            //   checkSleep();
        }
    }
    return 0;
}