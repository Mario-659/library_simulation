#include <iomanip>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <ncurses.h>
#include <unistd.h>
#include <string>
#include <atomic>
#include <unordered_map>

using namespace std;

const int NUMBER_OF_SHELVES = 6;
const int NUMBER_OF_LIBRARIANS = 3;
const int NUMBER_OF_READERS = 7;
const int NUMBER_OF_COMPUTER_STATIONS = 2;
const int MAX_BOOKS_PER_GENRE = 5;
const int SIMULATION_TIME_SECONDS = 30;

vector<string> genres = {"Fiction", "Non-fiction", "Sci-Fi", "Fantasy", "Romance", "Horror"};

struct Shelf {
    unordered_map<string, int> booksPerGenre;
    Shelf() {
        for (const auto& genre : genres) {
            booksPerGenre[genre] = MAX_BOOKS_PER_GENRE;
        }
    }
};

atomic<int> totalBooks(NUMBER_OF_SHELVES * genres.size() * MAX_BOOKS_PER_GENRE);

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

        // Checking for readers that are waiting for help
        for (int i = 0; i < NUMBER_OF_READERS && !didWork; i++) {
            if (readerStatus[i] == "Waiting for librarian") {
                bool usingComputer = false;
                
                {
                    lock_guard<mutex> lock(computerStationsMutex);
                    if (computerStationsOccupied < NUMBER_OF_COMPUTER_STATIONS) {
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
                    didWork = true;
                }
            }
        }

        // Restocking shelves randomly if free
        if (!didWork) {
            int randomShelf = rand() % NUMBER_OF_SHELVES;
            string randomGenre = genres[rand() % genres.size()];

            lock_guard<mutex> lock(shelvesMutex[randomShelf]);
            if (shelves[randomShelf].booksPerGenre[randomGenre] < MAX_BOOKS_PER_GENRE) {
                librarianStatus[id] = "Restocking shelf " + to_string(randomShelf + 1) + " with genre " + randomGenre;
                shelves[randomShelf].booksPerGenre[randomGenre]++;
                totalBooks++;
                didWork = true;
                sleep(1);
            }
        }
    }
}



void reader(int id) {
    while (running) {
        readerStatus[id] = "Looking for books";
        sleep(rand() % 3 + 1);

        int booksToRent = rand() % 3 + 1; // Rent 1 to 3 books
        vector<string> genresToRent;
        for (int i = 0; i < booksToRent; ++i) {
            genresToRent.push_back(genres[rand() % genres.size()]);
        }

        bool foundAllBooks = true;
        vector<int> shelfIndices;

        for (const auto& genre : genresToRent) {
            bool foundBook = false;
            for (int i = 0; i < NUMBER_OF_SHELVES && !foundBook; i++) {
                lock_guard<mutex> lock(shelvesMutex[i]);
                if (shelves[i].booksPerGenre[genre] > 0) {
                    shelves[i].booksPerGenre[genre]--;
                    foundBook = true;
                    shelfIndices.push_back(i);
                }
            }
            if (!foundBook) {
                foundAllBooks = false;
                break;
            }
        }

        if (foundAllBooks) {
            readerStatus[id] = "Waiting for librarian";
            sleep(5);

            // Return the books to the shelves
            for (int index : shelfIndices) {
                lock_guard<mutex> lock(shelvesMutex[index]);
                shelves[index].booksPerGenre[genresToRent[rand() % genresToRent.size()]]++;
                totalBooks++;
            }
        } else {
            readerStatus[id] = "Didn't find all books";
            sleep(2);
            readerStatus[id] = "Not in library";
            sleep(rand() % 3 + 1);
            continue;
        }
    }
}

int getNumberOfWaitingReaders() {
    int count = 0;
    for (const auto &status : readerStatus) {
        if (status == "Waiting for librarian") {
            count++;
        }
    }
    return count;
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
        printw("Number of waiting readers: ");
        attroff(COLOR_PAIR(6));
        attron(COLOR_PAIR(2));
        printw("%d\n\n", getNumberOfWaitingReaders());
        attroff(COLOR_PAIR(2));

        attron(COLOR_PAIR(6));
        printw("Shelves Status:\n");
        attroff(COLOR_PAIR(6));

        // Display shelves header
        for (size_t i = 0; i < shelves.size(); i++) {
            printw("| %-12s", ("Shelf " + to_string(i + 1)).c_str());
        }
        printw("|\n");

        for (const auto& genre : genres) {
            for (size_t i = 0; i < shelves.size(); i++) {
                attron(COLOR_PAIR(1)); // Display all shelves with the same color
                printw("| %s: %-2d ", genre.c_str(), shelves[i].booksPerGenre[genre]);
                attroff(COLOR_PAIR(1));
            }
            printw("|\n");
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
