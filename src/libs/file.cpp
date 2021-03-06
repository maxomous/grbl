/*
 * file.cpp
 */

#include "file.h"

using namespace std;

// internal functions
 
string File::IThisDir() 
{
	// lock the mutex
	std::unique_lock<std::mutex> locker(m_mutex);
    char result[255];
    ssize_t count = readlink("/proc/self/exe", result, 255);
    string out = string(result, (count > 0) ? count : 0);
    return out.substr(0, out.find_last_of("/") + 1);
}

string File::ICombineDirPath(const string& dir, const string& name) {
	
	string str = dir;
	if(str.back() != '/')
		str += '/';
	str += name;
	return str;
}

void File::IWrite(const string& filename, const string& str) {
	// lock the mutex
	std::unique_lock<std::mutex> locker(m_mutex);
	
	fstream f(filename, ios::out);
	if (f.is_open()) {
		f << str;
		f.close();
	}
}

void File::IAppend(const string& filename, const string& str) {
	// lock the mutex 
	std::unique_lock<std::mutex> locker(m_mutex);

	fstream f(filename, ios::out | ios::app);
	if (f.is_open()) {
		f << str;
		f.close();
	}
}

/*	Usage:
	auto lambda = [](string& str) {
	cout << str << endl;
	}; 
	if(readFile("/home/pi/Desktop/New.nc", lambda)) {
	cout << "Error: Could not open file" << endl;
	}
*/

// reads a line of a file and invokes the callback function with a pointer to the string 
// returns 0 on success / 1 if unsuccessful
int File::IRead(const string& filename, const function<int(string&)>& func, size_t from = 1, size_t to = 0) 
{
	// lock the mutex
	std::unique_lock<std::mutex> locker(m_mutex);
	
	ifstream openFile(filename);
	if(!openFile.is_open()) {
		cout << "Error: Couldn't open file" << endl;
		return -1;
	}
	string output;
	uint n = 0;
	while(getline(openFile, output)) {
		if(++n < from)
			continue;
		if(to != 0 && n > to)
			break;
		if(func(output)) {
			cout << "Error: Cannot execute line of file" << endl;
			return -1;
		}
	}
	openFile.close();
	return 0;
}

int File::IGetNumLines(const string& filename) 
{
	// lock the mutex
	std::unique_lock<std::mutex> locker(m_mutex);
	
	ifstream openFile(filename);
	if(!openFile.is_open()) {
		cout << "Error: Couldn't open file" << endl;
		return -1;
	}
	string unused;
	int count = 0;
	while(getline(openFile, unused)) {
		count++;
	}
	openFile.close();
	return count;
}

// take a directory location and returns the files and directories within it in the vector files
// if extensions is not an empty string, it will only return files with given extensions (can be seperated by ',' e.g. "exe,ini")
// returns 1 on failure
int File::IGetFilesInDir(const string& location, const string& extensions, vector<filedesc_t>& files)
{
	// lock the mutex
	std::unique_lock<std::mutex> locker(m_mutex);
	
	auto buildExtensionsList = [](const string& ext) 
	{
		istringstream stream(ext);
		vector<string> list;
		// build vector of extensions
		for(string s; getline(stream, s, ','); ) {
			// strip out whitespace & add to vector
			s.erase(remove_if(s.begin(), s.end(), ::isspace), s.end());
			list.push_back(s);
		}
		return list;
	};
	
	auto isValidExtension = [](const string& ext, vector<string> list)
	{	// returns true if ext is part of list
		for(string s : list) {
			if (ext == s)
			return true;
		}
		return false;
	};
	
	files.clear();
	vector<string> permittedExt = buildExtensionsList(extensions);
	
	DIR *dir;
	struct dirent *ent;
	
	if ((dir = opendir (location.c_str())) != NULL) {
		while ((ent = readdir (dir)) != NULL) {
			// ignore . hidden files & ..
			if(strncmp(ent->d_name, ".", 1) && strncmp(ent->d_name, "..", 2)) 
			{
				static int id = 0;
				string str = ent->d_name;
				string name, ext;
				int type = 0;
				// if directory
				if(ent->d_type == DT_DIR) {
					name = str;
					ext = "";
					type = FOLDER_TYPE;
				} else {
					// split name and file extension
					size_t dotPos = str.find_last_of(".");
					name = (dotPos != string::npos) ? str.substr(0, dotPos) : str;
					ext = (dotPos != string::npos) ? str.substr(dotPos+1) : "";
					type = FILE_TYPE;
				}
				if(type == FOLDER_TYPE || extensions == "" || isValidExtension(ext, permittedExt))
				{
					struct stat attrib;
					tm modtimeLocal;
					char dateStr[32];
					string filepath = CombineDirPath(location, string(ent->d_name));
					// get modified date & time
					if(!stat(filepath.c_str(), &attrib)) {
					time_t modtime = attrib.st_mtime;
					modtimeLocal = *localtime(&modtime);
					strftime(dateStr, 32, "%d-%b-%y  %H:%M", localtime(&modtime));
					}
					files.push_back((filedesc_t){.id = id++, .name = name, .ext = ext, .lastModified = modtimeLocal, .lastModifiedStr = dateStr, .type = type});
				}
			}
		}
		closedir (dir);
	} else {
		/* could not open directory */
		perror ("");
		return 1;
	}
	return 0;
}
