#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <ncurses.h>
#include <unistd.h>
#include <atomic>
#include <cstdlib>
#include <string>

std::mutex mtx;
std::atomic<bool> running(true);

struct Shelf {
    bool isOccupied = false;
    int numberOfBooks = rand() % 10 + 1;
};

std::vector<Shelf> shelves(5);

std::mutex computerMtx;
std::atomic<bool> computerOccupied(false);

std::vector<std::string> librarianStatus(2, "Idle");
std::vector<std::string> readerStatus(10, "Idle");

void librarian(int librarianID) {
    while (running) {
        librarianStatus[librarianID] = "Waiting for computer";
        if (!computerOccupied) {
            computerMtx.lock();
            computerOccupied = true;
            computerMtx.unlock();
            
            librarianStatus[librarianID] = "Using computer";
            // Librarian uses the computer
            sleep(rand() % 3 + 2);

            // Librarian done using computer
            computerMtx.lock();
            computerOccupied = false;
            computerMtx.unlock();
            librarianStatus[librarianID] = "Idle";
        }

        sleep(rand() % 3 + 1);
    }
}

void reader(int readerID) {
    while (running) {
        readerStatus[readerID] = "Looking for book";
        int shelfNumber = rand() % shelves.size();

        mtx.lock();
        if (!shelves[shelfNumber].isOccupied && shelves[shelfNumber].numberOfBooks > 0) {
            shelves[shelfNumber].isOccupied = true;
            shelves[shelfNumber].numberOfBooks--;
            mtx.unlock();
            
            readerStatus[readerID] = "Reading";
            sleep(rand() % 3 + 2); // reading

            // Return the book
            mtx.lock();
            shelves[shelfNumber].numberOfBooks++;
            shelves[shelfNumber].isOccupied = false;
            mtx.unlock();
            readerStatus[readerID] = "Idle";
        } else {
            mtx.unlock();
        }
        sleep(rand() % 3 + 1);
    }
}

void monitoring() {
    initscr();
    nodelay(stdscr, TRUE);
    noecho();
    curs_set(0);

    while (running) {
        clear();
        printw("LIBRARY SIMULATION\n\n");

        printw("Shelves Status:\n");
        for (size_t i = 0; i < shelves.size(); i++) {
            printw("Shelf %ld: %s, Number of Books: %d\n", i + 1, shelves[i].isOccupied ? "Occupied" : "Free", shelves[i].numberOfBooks);
        }

        printw("\nComputer Station: %s\n", computerOccupied ? "Occupied" : "Free");
        
        printw("\nLibrarians Status:\n");
        for (size_t i = 0; i < librarianStatus.size(); i++) {
            printw("Librarian %ld: %s\n", i + 1, librarianStatus[i].c_str());
        }
        
        printw("\nReaders Status:\n");
        for (size_t i = 0; i < readerStatus.size(); i++) {
            printw("Reader %ld: %s\n", i + 1, readerStatus[i].c_str());
        }

        refresh();
        sleep(1);
    }

    endwin();
}

int main() {
    std::vector<std::thread> librarians;
    for (int i = 0; i < 2; i++) {
        librarians.push_back(std::thread(librarian, i));
    }

    std::vector<std::thread> readers;
    for (int i = 0; i < 10; i++) {
        readers.push_back(std::thread(reader, i));
    }

    std::thread monitor(monitoring);

    monitor.join();
    for (auto& lib : librarians) lib.join();
    for (auto& reader : readers) reader.join();
    
    return 0;
}
