#include <iostream>
#include <Windows.h>
#include <list>
#include <thread>
#include <mutex>
#include <vector>
#include <sstream>
using namespace std;

mutex mtx;
condition_variable cv;
int minP = INT_MAX;
int maxP = 0;
int filesProccesed = 0;
list<wstring> filePathBuffer;
int totalFiles = 0; 
bool done= false;
mutex filesProcessedMutex;

void showProgress() {
    system("cls");
    cout << "filesprocessed (" << filesProccesed << "/" << totalFiles << ")" << endl;

}
bool isPrime(int number) {
    if (number <= 1) {
        return false;
    }
    if (number <= 3) {
        return true;
    }
    if (number % 2 == 0 || number % 3 == 0) {
        return false;
    }
    for (int i = 5; i * i <= number; i += 6) {
        if (number % i == 0 || number % (i + 2) == 0) {
            return false;
        }
    }
    return true;
}


void processFile(const wstring& filePath) {
    HANDLE hFile = CreateFile(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        cout << "Error opening file: " << GetLastError() << endl;
        return;
    }

    char buffer[256];
    DWORD bytesRead;

    while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        stringstream ss(buffer);
        int num;
        while (ss >> num) {
            if (isPrime(num)) {
                if (num > maxP) {
                    maxP = num;
                }
                if (num < minP) {
                    minP = num;
                }
            }
        }
    }

    CloseHandle(hFile);

    {
        lock_guard<mutex> lock(filesProcessedMutex);
        filesProccesed++;
        if (filesProccesed == totalFiles) {
            lock_guard<mutex> lock(mtx);
            done = true;
            cv.notify_all(); 
        }
    }

    {
        lock_guard<mutex> lock(mtx);
        if (filesProccesed%100==0) {
            showProgress();
    
        }
    }
}




void producer(const string& dir) {
    wstring wDir(dir.begin(), dir.end());
    wstring wPath = wDir + L"\\*";
    WIN32_FIND_DATA fileData;
    HANDLE hFind = FindFirstFile(wPath.c_str(), &fileData);

    if (hFind == INVALID_HANDLE_VALUE) {
        cout << "Error reading directory" << endl;
        return;
    }

    do {
        if (!(fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            wstring wsFileName(fileData.cFileName);
            wstring filePath = wDir + L"\\" + wsFileName;

            {
                lock_guard<mutex> lock(mtx);
                filePathBuffer.push_back(filePath);
            }

            cv.notify_one();
            totalFiles++;
        }
    } while (FindNextFile(hFind, &fileData) != 0);
 
    cout << totalFiles << endl;
    FindClose(hFind);
   
}

void consumer() {
    while (true) {
        wstring filePath;
        {
            unique_lock<mutex> lock(mtx);
            cv.wait(lock, [] { return !filePathBuffer.empty() || done; });
            if (filePathBuffer.empty() && done) {
                return; 
            }
            if (!filePathBuffer.empty()) {
                filePath = filePathBuffer.front();
                filePathBuffer.pop_front();
            }
        }

        if (!filePath.empty()) {
            processFile(filePath);
        }
    }
}


int main() {
    string dir = "C:\\Users\\Pijus\\source\\repos\\sisteminisld3\\rand_files";
    thread producerThread(producer, dir);
    vector<thread> consumerThreads;
    for (int i = 0; i < 4; ++i) {
        consumerThreads.emplace_back(consumer);
    }
    producerThread.join();
    for (thread& consumer : consumerThreads) {
        consumer.join();
    }
    cout << "Minimum: " << minP << " | Maximum: " << maxP << endl;

    return 0;
}
