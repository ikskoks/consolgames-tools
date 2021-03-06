#include <Archive.h>
#include <iostream>
#include <set>
#include <list>

using namespace std;
using namespace Consolgames;
using namespace ShatteredMemories;

void printUsage()
{
	cout
		<< "ArcTool for Shattered Memories by consolgames.ru\n"
		<< "Usage:" << endl
		<< "  ArcTool.exe <-e, --extract> <ArcFile> <OutDir> [names or hashes]" << endl
		<< "    Extract files from archive."  << endl
		<< "  ArcTool.exe <-r, --rebuild> <ArcFile> <OutArcFile> [input dirs]" << endl
		<< "    Rebuild archive using files from input dirs." << endl;
}

static bool isCommand(const char* command, const char* shortStr, const char* fullStr)
{
	return (command != NULL && (strcmp(command, shortStr) == 0 || strcmp(command, fullStr) == 0));
}

list<string> fileList(int argc, const char* argv[])
{
	list<string> names;
	for (int i = 4; i < argc; i++)
	{
		names.push_back(argv[i]);
	}
	return names;
}

vector<wstring> dirList(int argc, const char* argv[])
{
	vector<wstring> dirs;
	dirs.reserve(argc - 4);

	for (int i = 4; i < argc; i++)
	{
		const string dir = argv[i];
		const wstring dirw(dir.begin(), dir.end());
		dirs.push_back(dirw);
	}
	return dirs;
}

set<uint32> hashList(int argc, const char* argv[])
{
	set<uint32> hashes;
	for (int i = 4; i < argc; i++)
	{
		const char* str = argv[i];
		char* end = NULL;
		const uint32 hash = strtoul(str, &end, 16);
		if (*end != 0)
		{
			hashes.insert(Hash::calc(str));
		}
		else
		{
			hashes.insert(hash);
		}
	}
	return hashes;
}

class ProgressNotifier: public Archive::IProgressListener
{
public:
	ProgressNotifier()
		: m_currentCount(0)
	{
	}

private:
	virtual void startProgress(int maxValue) override
	{
		m_currentCount = maxValue;
	}
	virtual void progress(int value, const std::string& name) override
	{
		std::cout << "[" << (value + 1) << "/" << m_currentCount << "] " << name << "..." << std::endl;
	}
	virtual void finishProgress() override
	{	
	}
	virtual bool stopRequested() override
	{
		return false;
	}

private:
	int m_currentCount;
};

static ProgressNotifier s_defaultNotifier;

int main(int argc, const char* argv[])
{
	const char* cmd = argc > 1 ? argv[1] : NULL;

	if (isCommand(cmd, "-e", "--extract"))
	{
		const string filename = argv[2];
		Archive arc(filename);
		arc.addProgressListener(&s_defaultNotifier);
		if (!arc.open())
		{
			cout << "Unable to open arc file!" << endl;
			return -1;
		}

		const string outDir = argv[3];
		const set<uint32> hashes = hashList(argc, argv);
		const list<string> names = fileList(argc, argv);
		arc.setNames(names);
		if (!arc.extractFiles(outDir, hashes))
		{
			cout << "Unable to extract files!" << endl;
			return -1;
		}

		cout << "Done!" << endl;
	}
	else if (isCommand(cmd, "-r", "--rebuild"))
	{
		const string filename = argv[2];
		Archive arc(filename);
		arc.addProgressListener(&s_defaultNotifier);
		if (!arc.open())
		{
			cout << "Unable to open arc file!" << endl;
			return -1;
		}

		const string outFileA = argv[3];
		const wstring outFile(outFileA.begin(), outFileA.end());

		const vector<wstring> dirs = dirList(argc, argv);

		if (!arc.rebuild(outFile, dirs))
		{
			cout << "Unable to rebuild archive!";
			return false;
		}

		cout << "Done!" << endl;
	}
	else
	{
		if (argc > 1)
		{
			cout << "Invalid usage! Run program without arguments to see available commands." << endl;
			return -1;
		}
		printUsage();
	}

	return 0;
}
