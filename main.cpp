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

#define VIEWMODE_SPLIT 1
#define VIEWMODE_UNIFIED 2
//#define DIFFMODE_CHARACTER 1
//#define DIFFMODE_WORD 2

// members
int viewmode = VIEWMODE_UNIFIED;
//int diffmode = DIFFMODE_CHARACTER;
bool ignorecase = false;
bool ignorewhitespace = false;
//bool collapselines = true;
//bool tofile = false;

const char* filepath1 = nullptr;
const char* filepath2 = nullptr;

// repurposing unused ascii characters for special meaning
#define CHAR_ADD '\1' // start of heading
#define CHAR_REM '\2' // start of text
#define CHAR_END '\3' // end of text

#define CHAR_ADDB '\4' // end of transmission
#define CHAR_REMB '\5' // enquiry
#define CHAR_ENDB '\6' // ack
#define CHAR_NEUB '\14' // shift out



//#define CHAR_ADD 'A' // start of heading
//#define CHAR_REM 'R' // start of text
//#define CHAR_END 'E' // end of text
//
//#define CHAR_ADDB 'a' // end of transmission
//#define CHAR_REMB 'r' // enquiry
//#define CHAR_ENDB 'e' // ack
//#define CHAR_NEUB 'n' // shift out


void readparams(int argc, char** argv)
{
    filepath1 = argv[1];
    filepath2 = argv[2];

	for (int i = 3; i < argc; i++)
    {
        //if      (_strcmpi(argv[i], "tofile")           == 0) tofile       = true;
        if      (_strcmpi(argv[i], "unifiedview")      == 0) viewmode = VIEWMODE_UNIFIED;
        else if (_strcmpi(argv[i], "splitview")        == 0) viewmode = VIEWMODE_SPLIT;
        else if (_strcmpi(argv[i], "ignorewhitespace") == 0) ignorewhitespace = true;
        else if (_strcmpi(argv[i], "ignorecase")       == 0) ignorecase = true;
    }
}

void readfile(const char* filepath, string& str)
{
    if (filepath != nullptr)
    {
        ifstream t(filepath);
        str = string((istreambuf_iterator<char>(t)), istreambuf_iterator<char>());
    }
}

void writefile(const char* filepath, const string& str)
{
    if (filepath != nullptr)
    {
        ofstream outfile;
        outfile.open(filepath);
        outfile << str;
        outfile.close();
    }
}

int max3(int a, int b, int c) { return max(a, max(b, c)); }

#define UP 1
#define LEFT 2
#define DIAG 4

void adddelimiters(stringstream& ss, int mode, int prevmode)
{
    if (mode != prevmode)
    {
        // close previous
        if      (prevmode == LEFT) ss << CHAR_ADD;
        else if (prevmode == UP  ) ss << CHAR_REM;

        // open new
        if (mode == LEFT || mode == UP) ss << CHAR_END;
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
        bool cangoUp   = T[i][j] == T[i-1][j];
        bool cangoLeft = T[i][j] == T[i][j-1];
        bool cangoDiag = A[i-1]==B[j-1]; // T[i][j] == T[i-1][j-1]+1 &&

        // prefer to keep doing what it was doing before (up or left)
        if      (prevmode == UP   && cangoUp  ) mode = UP;
        else if (prevmode == LEFT && cangoLeft) mode = LEFT;
        else if (prevmode == DIAG && cangoDiag) mode = DIAG;
        else
        {
            mode = cangoLeft ? LEFT : (cangoUp ? UP : DIAG);
            //mode = cangoDiag ? DIAG : (cangoLeft ? LEFT : UP);
        }

        adddelimiters(ss, mode, prevmode);
		if      (mode == UP  )     ss << A[--i];
        else if (mode == LEFT)     ss << B[--j];
        else  /*(mode == DIAG)*/ { ss << A[--i]; --j; }
    }
    // do the final stretch
    if (i > 0)
    {
        while (i > 0) ss << A[--i];
        ss << CHAR_REM;
    }
    else if (j > 0)
    {
        while (j > 0) ss << B[--j];
        ss << CHAR_ADD;
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

void replaceall(string& str, char from, const string& to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != string::npos)
    {
        str.replace(start_pos, 1, to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
}

void replaceall(string& str, const string& from, const string& to)
{
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
}

void replaceone(string& str, const string& from, const string& to)
{
    size_t start_pos = 0;
    if ((start_pos = str.find(from, start_pos)) != string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
}

void removeall(string& str, char from, char to)
{
    size_t start_pos = 0, end_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != string::npos)
    {
        if ((end_pos = str.find(to, start_pos)) != string::npos)
        {
            str.erase(start_pos, end_pos - start_pos+1);
        }
        else
        {
            return;
        }
    }
}

void converttags(string& s) // TODO: single pass
{
    replaceall(s, CHAR_ADD, "<span class=ah>"); // add highlight
    replaceall(s, CHAR_REM, "<span class=rh>"); // rem highlight
    replaceall(s, CHAR_END, "</span>");

    replaceall(s, CHAR_ADDB, "<div class=ab>"); // add background
    replaceall(s, CHAR_REMB, "<div class=rb>"); // rem background
    replaceall(s, CHAR_NEUB, "<div class=nb>"); // neutral background
    replaceall(s, CHAR_ENDB, "</div>");
}

bool contains(const string& str, char c)
{
    return (str.find(c, 0) != string::npos);
}

string convert2html(string& s)
{
    //replace(s, "\n", "<br>");
    //replace(s, " ", "&nbsp;");
    replaceall(s, '<', "&lt;");
    replaceall(s, '>', "&gt;");

    if (viewmode == VIEWMODE_SPLIT)
    {
        // these add the infamous grey empty lines (not sure why)
        replaceall(s, "\1\n", "\n\1");
        replaceall(s, "\2\n", "\n\2");
        replaceall(s, "\n\3", "\3\n");

        string sl = s;
        string sr = s;

        replaceall(sl, CHAR_ADD, "\14 \6\1");
        replaceall(sr, CHAR_REM, "\14 \6\2");
        removeall(sl, CHAR_ADD, CHAR_END);
        removeall(sr, CHAR_REM, CHAR_END);

        // iterate over lines, add backgrounds, create lines
        converttags(sl);
        converttags(sr);



        string html;
        readfile("x:/Vs/Differ/template_split.html", html);
        replaceone(html, "__CODE_R__", sr); // in reverse order for speed
        replaceone(html, "__CODE_L__", sl);
        replaceone(html, "__LINE_R__", "_");
        replaceone(html, "__LINE_L__", "_");
        return html;
    }
    else if (viewmode == VIEWMODE_UNIFIED)
    {
        replaceall(s, "\1\n", "\n\1");
        replaceall(s, "\2\n", "\n\2");
        replaceall(s, "\n\3", "\3\n");
        
        
        std::stringstream ssi(s);
        std::string line;

        stringstream sso;
        while (std::getline(ssi, line, '\n'))
        {
            if (contains(line, CHAR_END))
            {
                sso << CHAR_NEUB << line << CHAR_ENDB << "\n";
            }
            else
            {
                sso << line << "\n";
            }
        }
        s = sso.str(); // output

        converttags(s);

        string html;
        readfile("x:/Vs/Differ/template_unified.html", html);
        replaceone(html, "__CODE__", s); // in reverse order for speed
        replaceone(html, "__LINE_R__", "_");
        replaceone(html, "__LINE_L__", "_");
        return html;
    }
}


#define TIMER_START \
    chrono::duration<double, milli> float_ms; \
    auto starttime = chrono::high_resolution_clock::now(); \
    auto endtime   = chrono::high_resolution_clock::now();

#define TIMER_END(msg) \
    endtime = chrono::high_resolution_clock::now(); \
    float_ms = endtime - starttime; \
    out << msg << " in " << float_ms.count() << " ms" << endl; \
    starttime = endtime;


ostream& getout()
{
    //if (tofile)
    //{
    //    ofstream outfile;
    //    outfile.open("differ_output.txt");
    //    return outfile;
    //}
    return cout;
}

int main(int argc, char** argv)
{
    ostream& out = cout;// = outfile;
	out << "Differ - (c) 2022 Simone Guggiari" << endl << endl;

    if (argc < 3)
    {
        cout << "Too few parameters. Usage: differ.exe file1.txt file2.txt [options]";
        return 1;
    }

    readparams(argc, argv);
    //for (int i = 0; i < argc; ++i)
    //{
    //    out << "argv[" << i << "]=" << argv[i] << "\n";
    //}
    
    if (filepath1 == nullptr || filepath2 == nullptr)
    {
        return 1;
    }

    TIMER_START;

    // read the files
    string file1, file2;
    readfile(filepath1, file1);
    readfile(filepath2, file2);
    out << "f1.size=" << file1.size() << " f2.size=" << file2.size() << endl;
    TIMER_END("read");

    writefile("output/differ_file1.txt", file1);
    writefile("output/differ_file2.txt", file2);

    // run the diff
    string diffresult = diff(file1, file2);
    TIMER_END("diff");

    // convert to html
    string diffresulthtml = convert2html(diffresult);
    TIMER_END("html");

    // generate the html file
    const char* outputfile = "x:/Vs/Differ/diffres.html";
    writefile(outputfile, diffresulthtml);
    TIMER_END("writ");

    // open it
    ShellExecuteA(NULL, "open", outputfile, NULL, NULL, SW_SHOWNORMAL);
    TIMER_END("open");

    //system("PAUSE");
	return 0;
}