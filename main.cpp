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
    start_color(); // Allows the usage of colors

    // Define color pairs
    init_pair(1, COLOR_GREEN, COLOR_BLACK); // Free shelf
    init_pair(2, COLOR_RED, COLOR_BLACK); // Occupied shelf
    init_pair(3, COLOR_YELLOW, COLOR_BLACK); // Computer status
    init_pair(4, COLOR_CYAN, COLOR_BLACK); // Librarian status
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK); // Reader status
    init_pair(6, COLOR_WHITE, COLOR_BLACK); // Headers

    nodelay(stdscr, TRUE);
    noecho();
    curs_set(0);

    while (running) {
        clear();
        attron(COLOR_PAIR(6)); // Set header color
        printw("LIBRARY SIMULATION\n\n");
        attroff(COLOR_PAIR(6)); // Turn off header color

        attron(COLOR_PAIR(6)); // Set header color
        printw("Shelves Status:\n");
        attroff(COLOR_PAIR(6)); // Turn off header color
        for (size_t i = 0; i < shelves.size(); i++) {
            attron(COLOR_PAIR(shelves[i].isOccupied ? 2 : 1)); // Set color based on shelf status
            printw("Shelf %ld: %s, Number of Books: %d\n", i + 1, shelves[i].isOccupied ? "Occupied" : "Free", shelves[i].numberOfBooks);
            attroff(COLOR_PAIR(shelves[i].isOccupied ? 2 : 1)); // Turn off color
        }

        attron(COLOR_PAIR(6)); // Set header color
        printw("\nComputer Station: ");
        attroff(COLOR_PAIR(6)); // Turn off header color
        attron(COLOR_PAIR(3)); // Set computer color
        printw("%s\n", computerOccupied ? "Occupied" : "Free");
        attroff(COLOR_PAIR(3)); // Turn off computer color

        attron(COLOR_PAIR(6)); // Set header color
        printw("\nLibrarians Status:\n");
        attroff(COLOR_PAIR(6)); // Turn off header color
        for (size_t i = 0; i < librarianStatus.size(); i++) {
            attron(COLOR_PAIR(4)); // Set librarian color
            printw("Librarian %ld: %s\n", i + 1, librarianStatus[i].c_str());
            attroff(COLOR_PAIR(4)); // Turn off librarian color
        }

        attron(COLOR_PAIR(6)); // Set header color
        printw("\nReaders Status:\n");
        attroff(COLOR_PAIR(6)); // Turn off header color
        for (size_t i = 0; i < readerStatus.size(); i++) {
            attron(COLOR_PAIR(5)); // Set reader color
            printw("Reader %ld: %s\n", i + 1, readerStatus[i].c_str());
            attroff(COLOR_PAIR(5)); // Turn off reader color
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
