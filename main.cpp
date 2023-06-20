#include <deque>
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
const int INITIAL_BOOKS_PER_GENRE = 2;
const int MAX_BOOKS_PER_GENRE = 5;
const int SIMULATION_TIME_SECONDS = 30;

vector<string> genres = {"Fiction", "Non-fict.", "Sci-Fi", "Fantasy", "Romance", "Horror"};

struct Book {
    string title;
    string author;
    string genre;
    int ratingSum = 0;
    int ratingCount = 0;
};

struct Shelf {
    unordered_map<string, vector<Book>> booksPerGenre;
    
    Shelf() {
        booksPerGenre["Fiction"].push_back({"The Great Gatsby", "F. Scott Fitzgerald", "Fiction", 0, 0});
        booksPerGenre["Fiction"].push_back({"To Kill a Mockingbird", "Harper Lee", "Fiction", 0, 0});

        booksPerGenre["Non-fict."].push_back({"The Immortal Life of Henrietta Lacks", "Rebecca Skloot", "Non-fict.", 0, 0});
        booksPerGenre["Non-fict."].push_back({"In Cold Blood", "Truman Capote", "Non-fict.", 0, 0});

        booksPerGenre["Sci-Fi"].push_back({"Dune", "Frank Herbert", "Sci-Fi", 0, 0});
        booksPerGenre["Sci-Fi"].push_back({"Neuromancer", "William Gibson", "Sci-Fi", 0, 0});

        booksPerGenre["Fantasy"].push_back({"Harry Potter and the Sorcerer's Stone", "J.K. Rowling", "Fantasy", 0, 0});
        booksPerGenre["Fantasy"].push_back({"The Hobbit", "J.R.R. Tolkien", "Fantasy", 0, 0});

        booksPerGenre["Romance"].push_back({"Pride and Prejudice", "Jane Austen", "Romance", 0, 0});
        booksPerGenre["Romance"].push_back({"Outlander", "Diana Gabaldon", "Romance", 0, 0});

        booksPerGenre["Horror"].push_back({"Dracula", "Bram Stoker", "Horror", 0, 0});
        booksPerGenre["Horror"].push_back({"The Shining", "Stephen King", "Horror", 0, 0});
    }
};

atomic<int> totalBooks(NUMBER_OF_SHELVES * 2 * genres.size());

bool running = true;

vector<Shelf> shelves(NUMBER_OF_SHELVES, Shelf());
vector<string> librarianStatus(NUMBER_OF_LIBRARIANS, "Free");
vector<string> readerStatus(NUMBER_OF_READERS, "Not in library");
int computerStationsOccupied = 0;

mutex shelvesMutex[NUMBER_OF_SHELVES];
mutex computerStationsMutex;

atomic<int> readersWhoDidntFindBooks(20);

// A map of genres to a list of books and authors
unordered_map<string, vector<pair<string, string>>> booksDatabase = {
    {"Fiction", {{"The Great Gatsby", "F. Scott Fitzgerald"}, {"Pride and Prejudice", "Jane Austen"}}},
    {"Non-fict.", {{"Sapiens", "Yuval Noah Harari"}, {"Educated", "Tara Westover"}}},
    {"Sci-Fi", {{"Dune", "Frank Herbert"}, {"Neuromancer", "William Gibson"}}},
    {"Fantasy", {{"Harry Potter", "J.K. Rowling"}, {"Lord of the Rings", "J.R.R. Tolkien"}}},
    {"Romance", {{"Pride and Prejudice", "Jane Austen"}, {"Outlander", "Diana Gabaldon"}}},
    {"Horror", {{"Dracula", "Bram Stoker"}, {"The Shining", "Stephen King"}}}
};



mutex eventsMutex;
deque<string> recentEvents;

void logEvent(const string &event) {
    lock_guard<mutex> lock(eventsMutex);

    time_t rawTime;
    struct tm *timeInfo;
    char buffer[80];

    time(&rawTime);
    timeInfo = localtime(&rawTime);

    strftime(buffer, sizeof(buffer), "%H:%M:%S", timeInfo);
    string strTime(buffer);

    if (recentEvents.size() >= 10) {
        recentEvents.pop_front();
    }

    recentEvents.push_back(strTime + " - " + event);
}


void librarian(int id) {
    while (running) {
        bool didWork = false;

        // checking for readers that are waiting for help
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

                    stringstream logMessage;
                    logMessage << "Librarian " << (id + 1) << " is checking out book for reader " << (i + 1);
                    logEvent(logMessage.str());

                    {
                        lock_guard<mutex> lock(computerStationsMutex);
                        computerStationsOccupied--;
                    }

                    librarianStatus[id] = "Free";
                    readerStatus[i] = "Leaving with book";

                    logMessage.str("");
                    logMessage << "Reader " << (i + 1) << " is leaving with book";
                    logEvent(logMessage.str());

                    sleep(2);
                    readerStatus[i] = "Not in library";
                    didWork = true;
                }
            } else if (readerStatus[i] == "Returning book") {
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
                    librarianStatus[id] = "Processing returned book from reader " + to_string(i + 1);

                    stringstream logMessage;
                    logMessage << "Librarian " << (id) << " is processing returned book from reader " << (i + 1);
                    logEvent(logMessage.str());

                    sleep(rand() % 2 + 1);

                    int randomShelf = rand() % NUMBER_OF_SHELVES;
                    string randomGenre = genres[rand() % genres.size()];
                    lock_guard<mutex> lock(shelvesMutex[randomShelf]);
                    if (!shelves[randomShelf].booksPerGenre[randomGenre].empty()) {
                        shelves[randomShelf].booksPerGenre[randomGenre][0].ratingSum += rand() % 5 + 1;
                        shelves[randomShelf].booksPerGenre[randomGenre][0].ratingCount += 1;
                    }

                    {
                        lock_guard<mutex> lock(computerStationsMutex);
                        computerStationsOccupied--;
                    }

                    librarianStatus[id] = "Free";
                    readerStatus[i] = "Not in library";
                    sleep(2);
                    didWork = true;
                }
            }
        }
        
        if (!didWork) {
            int randomShelf = rand() % NUMBER_OF_SHELVES;
            string randomGenre = genres[rand() % genres.size()];

            lock_guard<mutex> lock(shelvesMutex[randomShelf]);
            if (shelves[randomShelf].booksPerGenre[randomGenre].size() < MAX_BOOKS_PER_GENRE && rand() % 2) {
                librarianStatus[id] = "Restocking shelf " + to_string(randomShelf + 1) + " with genre " + randomGenre;
                
                stringstream logMessage;
                logMessage << "Librarian " << (id + 1) << " is restocking shelf " << (randomShelf + 1) << " with genre " << randomGenre;
                logEvent(logMessage.str());

                // select a random book from the book database of the given genre
                int bookIndex = rand() % booksDatabase[randomGenre].size();
                string bookTitle = booksDatabase[randomGenre][bookIndex].first;
                string bookAuthor = booksDatabase[randomGenre][bookIndex].second;

                // add the selected book to the shelf
                shelves[randomShelf].booksPerGenre[randomGenre].push_back({bookTitle, bookAuthor, randomGenre, 0, 0});
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

        unordered_map<string, int> booksToRent;
        for (int i = 0; i < rand() % genres.size(); ++i) {
            booksToRent[genres[rand() % genres.size()]] = rand() % 2 + 1;
        }

        stringstream logMessage;
        int totalBooks = 0;
        for (const auto& [genre, numBooks] : booksToRent) {
            totalBooks += numBooks;
        }
        logMessage << "Reader " << (id) << " wants to rent " << totalBooks << " books";
        logEvent(logMessage.str());


        bool foundAllBooks = true;
        vector<pair<int, pair<string, string>>> booksTaken;

        for (const auto& [genre, numBooks] : booksToRent) {
            int foundBooks = 0;
            // loop through shelves to find the books
            for (int i = 0; i < NUMBER_OF_SHELVES && foundBooks < numBooks; i++) {
                lock_guard<mutex> lock(shelvesMutex[i]);
                auto &books = shelves[i].booksPerGenre[genre];
                while (!books.empty() && foundBooks < numBooks) {
                    auto book = books.back();
                    books.pop_back();
                    booksTaken.push_back({i, {book.title, book.genre}});
                    foundBooks++;
                }
            }
            if (foundBooks < numBooks) {
                foundAllBooks = false;
                break;
            }
        }

        if (foundAllBooks) {
            readerStatus[id] = "Waiting for librarian";
            sleep(5);

            stringstream logMessage;
            logMessage << "Reader " << id << " has taken books: ";
            for (auto& [_, book] : booksTaken) {
                logMessage << book.first << ", ";
            }
            logEvent(logMessage.str());
            logMessage.str("");

            readerStatus[id] = "Reading books";

            sleep(rand() % 5 + 1);

            readerStatus[id] = "Returning book";
            

            logMessage << "Reader " << id << " is returning books";
            logEvent(logMessage.str());
            logMessage.str("");

            for (auto [index, book] : booksTaken) {
                lock_guard<mutex> lock(shelvesMutex[index]);
                auto &books = shelves[index].booksPerGenre[book.second];
                for (auto &b : books) {
                    if (b.title == book.first) {
                        int rating = rand() % 5 + 1;
                        b.ratingSum += rating;
                        b.ratingCount++;
                        break;
                    }
                }
            }
        } else {
            readerStatus[id] = "Returning partial books";

            // TODO the logic below is causing an exception
            // stringstream logMessage;
            // logMessage << "Reader " << (id) << " is returning partial books";
            // logEvent(logMessage.str());

            // // return the books that were taken
            // for (auto [index, book] : booksTaken) {
            //     lock_guard<mutex> lock(shelvesMutex[index]);
            //     shelves[index].booksPerGenre[book.second].push_back({book.first, book.second, 0, 0});
            // }

            sleep(2);
            readerStatus[id] = "Not in library";
            readersWhoDidntFindBooks++;
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

        // Display total number of books
        attron(COLOR_PAIR(6));
        printw("\nTotal Number of Books: ");
        attroff(COLOR_PAIR(6));
        attron(COLOR_PAIR(3));
        printw("%d\n", totalBooks.load());
        attroff(COLOR_PAIR(3));

        // Display number of waiting readers
        attron(COLOR_PAIR(6));
        printw("Number of waiting readers: ");
        attroff(COLOR_PAIR(6));
        attron(COLOR_PAIR(2));
        printw("%d", getNumberOfWaitingReaders());
        attroff(COLOR_PAIR(2));

        // Display number of readers who didn't find all books and returned home
        attron(COLOR_PAIR(6));
        printw("\nNumber of readers who didn't find all books and returned home: ");
        attroff(COLOR_PAIR(6));
        attron(COLOR_PAIR(2));
        printw("%d\n\n", readersWhoDidntFindBooks.load());
        attroff(COLOR_PAIR(2));

        // Display shelves status
        attron(COLOR_PAIR(6));
        printw("\nShelves Status:\n");
        attroff(COLOR_PAIR(6));

        // Display shelves header
        for (size_t i = 0; i < shelves.size(); i++) {
            printw("| %-16s", ("Shelf " + to_string(i + 1)).c_str());
        }
        printw("|\n");

        for (const auto& genre : genres) {
            for (size_t i = 0; i < shelves.size(); i++) {
                attron(COLOR_PAIR(1)); // Display all shelves with the same color
                // Get the count of books in this genre on this shelf
                int bookCount = shelves[i].booksPerGenre[genre].size();
                printw("| %-10s: %-3d ", genre.c_str(), bookCount);
                attroff(COLOR_PAIR(1));
            }
            printw("|\n");
        }

        // Display computer stations status
        attron(COLOR_PAIR(6));
        printw("\nComputer Stations: ");
        attroff(COLOR_PAIR(6));
        attron(COLOR_PAIR(3));
        printw("%d/%d\n", computerStationsOccupied, NUMBER_OF_COMPUTER_STATIONS);
        attroff(COLOR_PAIR(3));

        // Display librarians status
        attron(COLOR_PAIR(6));
        printw("\nLibrarians Status:\n");
        attroff(COLOR_PAIR(6));
        for (size_t i = 0; i < librarianStatus.size(); i++) {
            attron(COLOR_PAIR(4));
            printw("Librarian %ld: %s\n", i + 1, librarianStatus[i].c_str());
            attroff(COLOR_PAIR(4));
        }

        // Display readers status
        attron(COLOR_PAIR(6));
        printw("\nReaders Status:\n");
        attroff(COLOR_PAIR(6));
        for (size_t i = 0; i < readerStatus.size(); i++) {
            attron(COLOR_PAIR(5));
            printw("Reader %ld: %s\n", i + 1, readerStatus[i].c_str());
            attroff(COLOR_PAIR(5));
        }

        // Display recent events
        attron(COLOR_PAIR(6));
        printw("\nRecent Events:\n");
        attroff(COLOR_PAIR(6));

        lock_guard<mutex> lock(eventsMutex);
        for (const auto &event : recentEvents) {
            attron(COLOR_PAIR(1));
            printw("%s\n", event.c_str());
            attroff(COLOR_PAIR(1));
        }

        refresh();
        sleep(1);
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
