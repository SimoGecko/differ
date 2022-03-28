#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <streambuf>
#include <windows.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <chrono>

using namespace std;

//const char* htmltemplate = R"(...)";

// members
int diffmode = 1; // 0 == word , 1 == character
int viewmode = 1; // 0 == split, 1 == unified
bool ignorecase = false;
bool ignorewhitespace = true;
bool collapselines = true;
bool tofile = false;

const char* filepath1 = nullptr;
const char* filepath2 = nullptr;

void readparams(int argc, char** argv)
{
    filepath1 = argv[1];
    filepath2 = argv[2];

	for (int i = 3; i < argc; i++)
    {
        if      (_strcmpi(argv[i], "tofile")        == 0) tofile       = true;
        //else if (_strcmpi(argv[i], "value")        == 0 && (i + 1 < argc)) value        = std::stoi(argv[++i]);
    }
}

void readfile(const char* filepath, string& str)
{
    //std::ifstream t(filepath);
    //std::stringstream buffer;
    //buffer << t.rdbuf();
    //buffer.str(str);

    std::ifstream t(filepath);
    str = std::string((std::istreambuf_iterator<char>(t)),
        std::istreambuf_iterator<char>());
}

void writefile(const char* filepath, const string& str)
{
    ofstream outfile;
    outfile.open(filepath);
    outfile << str;
    outfile.close();
}

//char* readfile(char* filepath)
//{
//    FILE* f = fopen(filepath, "rb");
//    fseek(f, 0, SEEK_END);
//    long fsize = ftell(f);
//    fseek(f, 0, SEEK_SET);  /* same as rewind(f); */
//
//    char* string = malloc(fsize + 1);
//    fread(string, fsize, 1, f);
//    fclose(f);
//
//    string[fsize] = 0;
//    return string;
//}


int max3(int a, int b, int c) { return max(a, max(b, c)); }

#define U 1 // up
#define L 2 // left
#define D 4 // diag

void adddelimiters(stringstream& ss, int mode, int prevmode)
{
    if (mode == prevmode) return;
    if (mode != prevmode)
    {
        // close previous
        if      (prevmode == L) ss << "{+";
        else if (prevmode == U) ss << "{-";

        // open new
        if (mode == L || mode == U) ss << ".}";
    }
}

string lcs(const string& A, const string& B) // naive but optimal first implementation
{
    int I = A.size();
    int J = B.size();
    vector<vector<int>> T(I+1, vector<int>(J+1, 0));
    for (int i = 1; i <= I; ++i)
    {
        for (int j = 1; j <= J; ++j)
        {
            int d = (A[i-1] == B[j-1]) ? 1 : 0;
            T[i][j] = max3(T[i-1][j], T[i][j-1], T[i-1][j-1] + d);
        }
    }
    // build-back
    int i = I, j = J;
    stringstream ss;
    int mode = -1;

    while (i > 0 && j > 0)
    {
        int prevmode = mode;
		if (T[i][j]==T[i-1][j])
        {
            // move up
            mode = U;
            adddelimiters(ss, mode, prevmode);
            ss << A[--i];
        }
        else if (T[i][j] == T[i][j-1])
        {
            // move left
            mode = L;
            adddelimiters(ss, mode, prevmode);
            ss << B[--j];
        }
        else
        {
            // move diagonally
            //assert(A[i] == B[j]);
            mode = D;
            adddelimiters(ss, mode, prevmode);
            ss << A[--i];
            --j;
        }
    }
    // do the final stretch
    if (i > 0)
    {
        while (i > 0) ss << A[--i];
        ss << "{-";
    }
    if (j > 0)
    {
        while (j > 0) ss << B[--j];
        ss << "{+";
    }

    // reverse it
    string ans = ss.str();
    reverse(ans.begin(), ans.end());

    return ans;
}


string diff(const string& s1, const string& s2)
{
    return lcs(s1, s2);
}

void replaceall(string& str, const string& from, const string& to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
}

string convert2html(string& s)
{
    //replace(s, "\n", "<br>");
    //replace(s, " ", "&nbsp;");
    // html-ify
    replaceall(s, "<", "&lt;");
    replaceall(s, ">", "&gt;");

    replaceall(s, "+{", "<span class=\"add\">");
    replaceall(s, "-{", "<span class=\"rem\">");
    replaceall(s, "}.", "</span>");


    // add html
    string html;// (htmltemplate);
    readfile("x:/Vs/Differ/template.html", html);
    replaceall(html, "__CODE__", s);
    return html;
}


#define TIMER_START \
    chrono::duration<double, std::milli> float_ms; \
    auto starttime = chrono::high_resolution_clock::now(); \
    auto endtime   = chrono::high_resolution_clock::now();

#define TIMER_END(msg) \
    endtime = chrono::high_resolution_clock::now(); \
    float_ms = endtime - starttime; \
    out << msg << " in " << float_ms.count() << " ms" << std::endl; \
    starttime = endtime;


//void runandtime(const char* title, void* func)
//{
//
//    // do work
//
//}


ostream& getout()
{
    if (tofile)
    {
        ofstream outfile;
        outfile.open("differ_output.txt");
        return outfile;
    }
    return cout;
}

int main(int argc, char** argv)
{
    // need at least 3 args
    if (argc < 3)
    {
        cout << "too few parameters. Usage: differ.exe file1.txt file2.txt [options]";
        return 1;
    }

    readparams(argc, argv);

    //ofstream outfile;
    //outfile.open("differ_output.txt");

    ostream& out = cout;// = outfile;

	out << "Differ" << endl;

    for (int i = 0; i < argc; ++i)
    {
        out << "argv[" << i << "]=" << argv[i] << "\n";
    }
    
    // read the files
    if (filepath1 == nullptr || filepath2 == nullptr)
    {
        return 1;
    }
    string file1, file2;

    TIMER_START;

    readfile(filepath1, file1);
    readfile(filepath2, file2);
    out << "f1.size=" << file1.size() << " f2.size=" << file2.size() << endl;
    TIMER_END("read");


    writefile("differ_file1.txt", file1);
    writefile("differ_file2.txt", file2);

    // run the diff
    string diffresult = diff(file1, file2);
    TIMER_END("diff");

    // convert to html
    string diffresulthtml = convert2html(diffresult);
    TIMER_END("html");

    // generate the html file
    writefile("diffres.html", diffresulthtml);
    TIMER_END("writ");

    // open it
    ShellExecute(NULL, L"open", L"diffres.html", NULL, NULL, SW_SHOWNORMAL);
    TIMER_END("open");
    //LPCTSTR helpFile = "c\help\helpFile.html";

    //int z; cin >> z;
    //system("PAUSE");
	return 0;
}