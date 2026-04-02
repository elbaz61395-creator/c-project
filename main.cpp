#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <memory>
#include <limits>
#include <SFML/Audio.hpp>

#if __has_include(<filesystem>)
  #include <filesystem>
  namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
  #include <experimental/filesystem>
  namespace fs = std::experimental::filesystem;
#else
  #error "Compiler needs filesystem support (C++17)."
#endif

#ifdef _WIN32
  #include <conio.h>
  #include <windows.h>
#else
  #include <termios.h>
  #include <unistd.h>
  #include <sys/select.h>
#endif

using namespace std;

#ifdef _WIN32
inline bool kbhit_wrap() { return _kbhit() != 0; }
#else
inline bool kbhit_wrap() {
    timeval tv{0,0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO+1, &fds, nullptr, nullptr, &tv) > 0;
}
#endif

#ifdef _WIN32
inline int getch_wrap() { return _getch(); }
#else
inline int getch_wrap() {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int c = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return c;
}
#endif

#ifdef _WIN32
inline void sleep_ms(int ms) { Sleep(ms); }
#else
inline void sleep_ms(int ms) { usleep(ms * 1000); }
#endif

int getInt(const string& prompt) {
    cout << prompt;
    int v;
    while (!(cin >> v)) {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Please enter a number: ";
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    return v;
}

string getLine(const string& prompt) {
    cout << prompt;
    string s;
    getline(cin, s);
    if (!s.empty() && s.back() == '\r') s.pop_back();
    return s;
}

struct SongNode {
    int id;
    string filename;
    string filepath;
    SongNode* next = nullptr;
    SongNode* prev = nullptr;
    SongNode(int i, const string& name, const string& path)
      : id(i), filename(name), filepath(path) {}
};

class Playlist {
private:
    SongNode* head = nullptr;
    SongNode* tail = nullptr;
    SongNode* current = nullptr;
    int nextId = 1;
    sf::Music music;

public:
    string name;
    Playlist() = default;
    Playlist(const string& n) : name(n) {}
    ~Playlist() { clear(); }
    void clear() {
        stop();
        SongNode* t = head;
        while (t) {
            SongNode* tmp = t->next;
            delete t;
            t = tmp;
        }
        head = tail = current = nullptr;
        nextId = 1;
    }
    bool empty() const { return head == nullptr; }
    void addSong(const string& filename, const string& filepath) {
        SongNode* n = new SongNode(nextId++, filename, filepath);
        if (!head) head = tail = n;
        else { tail->next = n; n->prev = tail; tail = n; }
    }
    void insertSongAt(int pos, const string& filename, const string& filepath) {
        if (pos <= 1 || !head) {
            SongNode* n = new SongNode(nextId++, filename, filepath);
            n->next = head;
            if (head) head->prev = n;
            head = n;
            if (!tail) tail = n;
            renumberIds();
            return;
        }
        SongNode* t = head;
        int idx = 1;
        while (t->next && idx < pos - 1) { t = t->next; ++idx; }
        if (!t->next) { addSong(filename, filepath); return; }
        SongNode* n = new SongNode(nextId++, filename, filepath);
        n->next = t->next; n->prev = t;
        t->next->prev = n; t->next = n;
        renumberIds();
    }
    SongNode* findById(int id) {
        for (SongNode* t = head; t; t = t->next) if (t->id == id) return t;
        return nullptr;
    }
    bool removeById(int id) {
        SongNode* t = findById(id);
        if (!t) return false;
        if (t->prev) t->prev->next = t->next; else head = t->next;
        if (t->next) t->next->prev = t->prev; else tail = t->prev;
        if (current == t) current = t->next ? t->next : head;
        delete t;
        renumberIds();
        return true;
    }
    void displaySongs(bool highlightCurrent = false) {
        cout << "\n--- Playlist: " << name << " ---\n";
        for (SongNode* t = head; t; t = t->next) {
            if (highlightCurrent && t == current) cout << " >> ";
            else cout << "    ";
            cout << "[" << t->id << "] " << t->filename << "\n";
        }
        if (!head) cout << " (empty)\n";
    }
    void renumberIds() {
        int id = 1;
        for (SongNode* t = head; t; t = t->next) t->id = id++;
        nextId = id;
    }
    bool moveSong(int id, int direction) {
        SongNode* t = findById(id);
        if (!t) return false;
        if (direction == -1 && t->prev) {
            SongNode* p = t->prev;
            swapNodes(p, t);
            renumberIds(); return true;
        } else if (direction == 1 && t->next) {
            SongNode* n = t->next;
            swapNodes(t, n);
            renumberIds(); return true;
        }
        return false;
    }
    void swapNodes(SongNode* a, SongNode* b) {
        if (!a || !b || a->next != b) return;
        SongNode* ap = a->prev;
        SongNode* bn = b->next;
        if (ap) ap->next = b; else head = b;
        if (bn) bn->prev = a; else tail = a;
        b->prev = ap; a->next = bn;
        b->next = a; a->prev = b;
    }
    void playById(int id) {
        SongNode* t = findById(id);
        if (!t) { cout << "Invalid ID\n"; return; }
        current = t; playCurrent();
    }
    void playCurrent() {
        if (!current) return;
        music.stop();
        if (!music.openFromFile(current->filepath)) {
            cout << "\n>> Error opening: " << current->filepath << "\n";
            return;
        }
        cout << "\n>> Now Playing [" << current->id << "] " << current->filename << "\n";
        music.play();
    }
    void nextSong() {
        if (!current) return;
        current = current->next ? current->next : head;
        playCurrent();
    }
    void prevSong() {
        if (!current) return;
        current = current->prev ? current->prev : tail;
        playCurrent();
    }
    void pauseMusic() {
        if (music.getStatus() == sf::Music::Playing) {
            music.pause(); cout << ">> Paused\n";
        }
    }
    void resumeMusic() {
        if (music.getStatus() == sf::Music::Paused) {
            music.play(); cout << ">> Resumed\n";
        }
    }
    void stop() { music.stop(); }
    bool isStopped() const { return music.getStatus() == sf::SoundSource::Stopped; }
    bool advanceToNextAndPlay() {
        if (!current && !head) return false;
        if (!current) current = head;
        else current = current->next ? current->next : head;
        playCurrent();
        return true;
    }
    SongNode* getCurrent() const { return current; }
    SongNode* getHead() const { return head; }
    SongNode* getTail() const { return tail; }
    bool saveToFile(const string& filepathOut) {
        ofstream out(filepathOut, ios::trunc);
        if (!out) return false;
        out << name << "\n";
        for (SongNode* t = head; t; t = t->next) out << t->filename << "\t" << t->filepath << "\n";
        return true;
    }
    bool loadFromFile(const string& filepathIn) {
        ifstream in(filepathIn);
        if (!in) return false;
        clear();
        string line;
        if (!getline(in, name)) return false;
        if (!name.empty() && name.back() == '\r') name.pop_back();
        while (getline(in, line)) {
            if (line.empty()) continue;
            if (line.back() == '\r') line.pop_back();
            auto tab = line.find('\t');
            string fname, fpath;
            if (tab != string::npos) {
                fname = line.substr(0, tab);
                fpath = line.substr(tab + 1);
            } else {
                fpath = line;
                fname = fs::path(fpath).filename().string();
            }
            addSong(fname, fpath);
        }
        renumberIds();
        return true;
    }
};

class PlaylistManager {
public:
    vector<unique_ptr<Playlist>> lists;
    vector<pair<string,string>> library;
    void scanLibrary(const string& folderName = ".") {
        library.clear();
        if (!fs::exists(folderName) || !fs::is_directory(folderName)) {
            cout << "\n[Error] Folder '" << folderName << "' not found or is not a directory.\n";
            return;
        }
        try {
            for (const auto& e : fs::directory_iterator(folderName)) {
                if (!e.is_regular_file()) continue;
                string ext = e.path().extension().string();
                transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".ogg" || ext == ".wav" || ext == ".mp3") {
                    library.emplace_back(e.path().filename().string(), e.path().string());
                }
            }
        } catch (...) {
            cout << "Error accessing folder.\n";
        }
        std::sort(library.begin(), library.end(), [](const pair<string,string> &a, const pair<string,string> &b){
            return a.first < b.first;
        });
    }
    void listLibrary() {
        cout << "\n--- Library Files ---\n";
        int i = 1;
        for (auto &p : library) cout << " [" << i++ << "] " << p.first << "\n";
        if (library.empty()) cout << " (no .ogg, .wav, or .mp3 files found)\n";
    }
    int createPlaylist(const string& name) {
        lists.push_back(make_unique<Playlist>(name));
        return (int)lists.size();
    }
    bool removePlaylist(int id) {
        if (id < 1 || id > (int)lists.size()) return false;
        lists.erase(lists.begin() + (id-1));
        return true;
    }
    void displayPlaylists() {
        cout << "\n--- Playlists ---\n";
        for (size_t i=0;i<lists.size();++i) cout << " [" << (i+1) << "] " << lists[i]->name << "\n";
        if (lists.empty()) cout << " (none)\n";
    }
    Playlist* getPlaylist(int id) {
        if (id < 1 || id > (int)lists.size()) return nullptr;
        return lists[id-1].get();
    }
};

void displayMainMenuBox() {
    cout << "\n+============================================================+\n";
    cout << "|                  Quraan Playlist Manager                   |\n";
    cout << "+============================================================+\n";
    cout << "|  1. Add a new playlist                                     |\n";
    cout << "|  2. Add Surah to an exisiting playlist                     |\n";
    cout << "|  3. Remove Surah from an exisiting playlist                |\n";
    cout << "|  4. Update the order of exisiting playlist                 |\n";
    cout << "|  5. Display All Current Playlists                          |\n";
    cout << "|  6. Display all Playlists Shurah                           |\n";
    cout << "|  7. Display Shurah in a specific playlist                  |\n";
    cout << "|  8. Play Shurah from specific playlist                     |\n";
    cout << "|     Use left arrow (<-) to play the previous shurah        |\n";
    cout << "|     Use right arrow (->) to play the next shurah           |\n";
    cout << "|     Use up arrow (^) to pause the current shurah           |\n";
    cout << "|     Use down arrow (v) to resume the current shurah        |\n";
    cout << "|     Press q to exit current playlist & return back to menu |\n";
    cout << "|  9. Save an existing playlist to a file                    |\n";
    cout << "| 10. Load an existing playlist from a file                  |\n";
    cout << "| 11. Remove an existing playlist                            |\n";
    cout << "| 12. Exit                                                   |\n";
    cout << "+============================================================+\n";
    cout << "Your choice: ";
}

void playPlaylistInteractive(Playlist* pl) {
    if (!pl || pl->empty()) { cout << "Playlist empty or invalid.\n"; return; }
    cout << "\nControls Active (Arrows / Q to quit)...\n";
    if (!pl->getCurrent()) {
        pl->playById(1);
    }
    int ch = 0;
    while (true) {
        if (pl->isStopped()) {
            SongNode* cur = pl->getCurrent();
            if (cur) {
                cout << "\n[Info] Finished: " << cur->filename << "\n";
                cout << "[Info] Moving to next Shurah...\n";
                sleep_ms(900);
                pl->advanceToNextAndPlay();
            }
        }
        if (!kbhit_wrap()) { sleep_ms(40); continue; }
        ch = getch_wrap();
#ifdef _WIN32
        if (ch == 0 || ch == 224) {
            int code = getch_wrap();
            if (code == 75) pl->prevSong();
            else if (code == 77) pl->nextSong();
            else if (code == 72) pl->pauseMusic();
            else if (code == 80) pl->resumeMusic();
        } else {
            if (ch == 'q' || ch == 'Q') { pl->stop(); break; }
        }
#else
        if (ch == 27) {
            if (!kbhit_wrap()) continue;
            int ch2 = getch_wrap();
            int ch3 = getch_wrap();
            if (ch3 == 'D') pl->prevSong();
            else if (ch3 == 'C') pl->nextSong();
            else if (ch3 == 'A') pl->pauseMusic();
            else if (ch3 == 'B') pl->resumeMusic();
        } else {
            if (ch == 'q' || ch == 'Q') { pl->stop(); break; }
        }
#endif
    }
}

int main() {
    PlaylistManager mgr;
    mgr.scanLibrary(".");
    while (true) {
        displayMainMenuBox();
        int choice;
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Invalid choice\n";
            sleep_ms(700);
            continue;
        }
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        if (choice == 12) break;
        switch (choice) {
            case 1: {
                string name = getLine("Enter new playlist name: ");
                if (name.empty()) name = "Playlist_" + to_string(mgr.lists.size()+1);
                int id = mgr.createPlaylist(name);
                cout << "Created playlist [" << id << "] " << name << "\n";
                break;
            }
            case 2: {
                // 1. البحث في المجلد (كما هو)
                string folderName = getLine("Enter folder name to scan (press Enter for current folder): ");
                if (folderName.empty()) folderName = ".";
                mgr.scanLibrary(folderName);

                if (mgr.library.empty()) {
                    cout << "No files found inside this folder.\n";
                    break;
                }

                // 2. التحقق من وجود قوائم تشغيل (كما هو)
                if (mgr.lists.empty()) {
                    cout << "No playlists. Create one first.\n";
                    break;
                }

                // 3. اختيار قائمة التشغيل (نقلناها للأعلى لتختارها مرة واحدة فقط)
                mgr.displayPlaylists();
                int pid = getInt("Choose playlist ID to add to: ");
                Playlist* pl = mgr.getPlaylist(pid);
                if (!pl) { cout << "Invalid playlist\n"; break; }

                // 4. عرض المكتبة وبدء حلقة الإضافة المتعددة
                mgr.listLibrary();
                cout << "\n--- Enter file numbers to add (Enter 0 to finish) ---\n";

                while (true) {
                    int libIndex = getInt("Choose library file number (0 to stop): ");

                    // شرط الخروج من التكرار
                    if (libIndex == 0) break;

                    // التحقق من صحة الرقم
                    if (libIndex < 1 || libIndex > (int)mgr.library.size()) {
                        cout << "Invalid file selection, try again.\n";
                        continue;
                    }

                    // الإضافة للقائمة
                    auto &entry = mgr.library[libIndex-1];
                    pl->addSong(entry.first, entry.second); // أو addSurah حسب تسمية دالتك
                    cout << " -> Added [" << entry.first << "] to " << pl->name << "\n";
                }

                cout << "Finished adding files.\n";
                break;
            }
            case 3: {
                mgr.displayPlaylists();
                if (mgr.lists.empty()) break;
                int pid = getInt("Playlist ID: ");
                Playlist* pl = mgr.getPlaylist(pid);
                if (!pl) { cout << "Invalid playlist\n"; break; }
                pl->displaySongs();
                int sid = getInt("Surah ID to remove: ");
                if (pl->removeById(sid)) cout << "Removed.\n"; else cout << "Invalid ID.\n";
                break;
            }
            case 4: {
                mgr.displayPlaylists();
                if (mgr.lists.empty()) break;
                int pid = getInt("Playlist ID: ");
                Playlist* pl = mgr.getPlaylist(pid);
                if (!pl) { cout << "Invalid playlist\n"; break; }
                pl->displaySongs();
                int sid = getInt("Surah ID to move: ");
                cout << "Move (1) Up or (2) Down? ";
                int dir;
                while (!(cin >> dir)) { cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n'); cout << "Enter 1 or 2: "; }
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                if (dir == 1) { if (pl->moveSong(sid, -1)) cout << "Moved up.\n"; else cout << "Cannot move up.\n"; }
                else if (dir == 2) { if (pl->moveSong(sid, 1)) cout << "Moved down.\n"; else cout << "Cannot move down.\n"; }
                else cout << "Invalid option\n";
                break;
            }
            case 5: {
                mgr.displayPlaylists();
                break;
            }
            case 6: {
                for (size_t i=0;i<mgr.lists.size();++i) {
                    cout << "\nPlaylist [" << (i+1) << "] " << mgr.lists[i]->name << "\n";
                    mgr.lists[i]->displaySongs();
                }
                if (mgr.lists.empty()) cout << " (no playlists)\n";
                break;
            }
            case 7: {
                mgr.displayPlaylists();
                if (mgr.lists.empty()) break;
                int pid = getInt("Playlist ID: ");
                Playlist* pl = mgr.getPlaylist(pid);
                if (!pl) { cout << "Invalid playlist\n"; break; }
                pl->displaySongs();
                break;
            }
            case 8: {
                mgr.displayPlaylists();
                if (mgr.lists.empty()) break;
                int pid = getInt("Playlist ID to play: ");
                Playlist* pl = mgr.getPlaylist(pid);
                if (!pl) { cout << "Invalid playlist\n"; break; }
                pl->displaySongs(true);
                int sid = getInt("Enter Surah ID to start from (or 0 to start from first): ");
                if (sid == 0) sid = 1;
                pl->playById(sid);
                playPlaylistInteractive(pl);
                break;
            }
            case 9: {
                mgr.displayPlaylists();
                if (mgr.lists.empty()) {
                    cout << "No playlists available to save.\n";
                    break;
                }
                int pid = getInt("Enter Playlist ID to save: ");
                Playlist* pl = mgr.getPlaylist(pid);
                if (!pl) {
                    cout << "Error: Invalid playlist ID.\n";
                    break;
                }
                string path = getLine("Enter filename (e.g., 'favorite'): ");
                if (path.empty()) {
                    cout << "Operation cancelled: No filename provided.\n";
                    break;
                }
                if (path.length() < 4 || path.substr(path.length() - 4) != ".txt") {
                    path += ".txt";
                }
                if (pl->saveToFile(path)) {
                    cout << ">> Success! Playlist saved to '" << path << "'\n";
                } else {
                    cout << ">> Error: Failed to save file. Check permissions.\n";
                }
                break;
            }
            case 10: {
                string path = getLine("Enter filename to load from: ");
                if (path.empty()) { cout << "No filename\n"; break; }
                auto pl = make_unique<Playlist>();
                if (!pl->loadFromFile(path)) { cout << "Failed to load.\n"; break; }
                cout << "Loaded playlist: " << pl->name << "\n";
                mgr.lists.push_back(move(pl));
                break;
            }
            case 11: {
                mgr.displayPlaylists();
                if (mgr.lists.empty()) break;
                int pid = getInt("Playlist ID to remove: ");
                if (mgr.removePlaylist(pid)) cout << "Removed playlist\n"; else cout << "Invalid playlist\n";
                break;
            }
            default:
                cout << "Unknown choice\n";
                break;
        }
        cout << "\n(Press Enter to continue)";
        cin.get();
    }
    cout << "Exiting.\n";
    return  0;
}