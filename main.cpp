#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <ncurses.h>
#include <unistd.h>
#include <string>
#include <atomic>

using namespace std;

const int NUMBER_OF_SHELVES = 6;
const int NUMBER_OF_LIBRARIANS = 3;
const int NUMBER_OF_READERS = 7;
const int NUMBER_OF_COMPUTER_STATIONS = 2;
const int MAX_BOOKS_PER_SHELF = 20;
const int SIMULATION_TIME_SECONDS = 30;

struct Shelf {
    int numberOfBooks;
    int maxNumberOfBooks;
    bool isOccupied;
    Shelf() : numberOfBooks(10), maxNumberOfBooks(MAX_BOOKS_PER_SHELF), isOccupied(false) {}
};

atomic<int> totalBooks(NUMBER_OF_SHELVES * 10);

bool running = true;

vector<Shelf> shelves(NUMBER_OF_SHELVES, Shelf());
vector<string> librarianStatus(NUMBER_OF_LIBRARIANS, "Free");
vector<string> readerStatus(NUMBER_OF_READERS, "Not in library");
int computerStationsOccupied = 0;

mutex shelvesMutex[NUMBER_OF_SHELVES];
mutex computerStationsMutex;

void librarian(int id) {
    while (running) {
        bool didWork = false;
        for (int i = 0; i < NUMBER_OF_READERS && !didWork; i++) {
            if (readerStatus[i] == "Waiting for librarian") {
                bool usingComputer = false;
                
                {
                    lock_guard<mutex> lock(computerStationsMutex);
                    if (computerStationsOccupied < NUMBER_OF_COMPUTER_STATIONS && readerStatus[i] == "Waiting for librarian") {
                        computerStationsOccupied++;
                        usingComputer = true;
                        readerStatus[i] = "Being helped";
                    }
                }

                if (usingComputer) {
                    librarianStatus[id] = "Checking out book for reader " + to_string(i + 1);
                    sleep(rand() % 2 + 1);

                    
                    {
                        lock_guard<mutex> lock(computerStationsMutex);
                        computerStationsOccupied--;
                    }

                    
                    librarianStatus[id] = "Free";
                    readerStatus[i] = "Leaving with book";
                    sleep(2);
                    readerStatus[i] = "Not in library";
                }
            }
        }

        // restocking shelves if free
        if (!didWork) {
            for (int i = 0; i < NUMBER_OF_SHELVES && !didWork; i++) {
                lock_guard<mutex> lock(shelvesMutex[i]);
                if (!shelves[i].isOccupied && shelves[i].numberOfBooks < shelves[i].maxNumberOfBooks) {
                    librarianStatus[id] = "Restocking shelf " + to_string(i + 1);
                    shelves[i].numberOfBooks++;
                    totalBooks++;
                    didWork = true;
                    sleep(1);
                }
            }
        }
    }
}


void reader(int id) {
    // sleep(rand() % 3 + 1);

    while (running) {
        readerStatus[id] = "Looking for book";
        sleep(rand() % 3 + 1);

        bool foundBook = false;
        int shelfIndex = -1;
        for (int i = 0; i < NUMBER_OF_SHELVES && !foundBook; i++) {
            lock_guard<mutex> lock(shelvesMutex[i]);
            if (!shelves[i].isOccupied && shelves[i].numberOfBooks > 0) {
                shelves[i].isOccupied = true;
                shelves[i].numberOfBooks--;
                foundBook = true;
                shelfIndex = i;
            }
        }

        // If found book, proceed to librarian
        if (foundBook) {
            readerStatus[id] = "Waiting for librarian";
            sleep(5);

            // Mark the shelf as unoccupied
            lock_guard<mutex> lock(shelvesMutex[shelfIndex]);
            shelves[shelfIndex].isOccupied = false;
        } else {
            readerStatus[id] = "Didn't find book";
            sleep(2);
            readerStatus[id] = "Not in library";
            sleep(rand() % 3 + 1);
            continue;
        }

        // Randomly decide if this reader will return a book
        if (rand() % 2 == 0 && foundBook) {
            readerStatus[id] = "Returning book";
            lock_guard<mutex> lock(shelvesMutex[shelfIndex]);
            shelves[shelfIndex].numberOfBooks++;
            totalBooks++;
            sleep(1);
        }
    }
}


void monitoring() {
    initscr();
    start_color();

    init_pair(1, COLOR_GREEN, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_CYAN, COLOR_BLACK);
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(6, COLOR_WHITE, COLOR_BLACK);

    nodelay(stdscr, TRUE);
    noecho();
    curs_set(0);

    while (running) {
        clear();
        attron(COLOR_PAIR(6));
        printw("LIBRARY SIMULATION\n\n");
        attroff(COLOR_PAIR(6));

        attron(COLOR_PAIR(6));
        printw("\nTotal Number of Books: ");
        attroff(COLOR_PAIR(6));
        attron(COLOR_PAIR(3));
        printw("%d\n", totalBooks.load());
        attroff(COLOR_PAIR(3));

        attron(COLOR_PAIR(6));
        printw("Shelves Status:\n");
        attroff(COLOR_PAIR(6));
        for (size_t i = 0; i < shelves.size(); i++) {
            attron(COLOR_PAIR(shelves[i].isOccupied ? 2 : 1));
            printw("Shelf %ld: %s, Number of Books: %d\n", i + 1, shelves[i].isOccupied ? "Occupied" : "Free", shelves[i].numberOfBooks);
            attroff(COLOR_PAIR(shelves[i].isOccupied ? 2 : 1));
        }

        attron(COLOR_PAIR(6));
        printw("\nComputer Stations: ");
        attroff(COLOR_PAIR(6));
        attron(COLOR_PAIR(3));
        printw("%d/%d\n", computerStationsOccupied, NUMBER_OF_COMPUTER_STATIONS);
        attroff(COLOR_PAIR(3));

        attron(COLOR_PAIR(6));
        printw("\nLibrarians Status:\n");
        attroff(COLOR_PAIR(6));
        for (size_t i = 0; i < librarianStatus.size(); i++) {
            attron(COLOR_PAIR(4));
            printw("Librarian %ld: %s\n", i + 1, librarianStatus[i].c_str());
            attroff(COLOR_PAIR(4));
        }

        attron(COLOR_PAIR(6));
        printw("\nReaders Status:\n");
        attroff(COLOR_PAIR(6));
        for (size_t i = 0; i < readerStatus.size(); i++) {
            attron(COLOR_PAIR(5));
            printw("Reader %ld: %s\n", i + 1, readerStatus[i].c_str());
            attroff(COLOR_PAIR(5));
        }

        refresh();
        usleep(500000);
    }

    endwin();
}


int main() {
    vector<thread> librarians;
    vector<thread> readers;

    for (int i = 0; i < NUMBER_OF_LIBRARIANS; i++) {
        librarians.push_back(thread(librarian, i));
    }

    for (int i = 0; i < NUMBER_OF_READERS; i++) {
        readers.push_back(thread(reader, i));
    }

    thread monitoringThread(monitoring);

    monitoringThread.join();

    running = false;
    for (auto& t : librarians) {
        t.join();
    }
    for (auto& t : readers) {
        t.join();
    }

    return 0;
}
