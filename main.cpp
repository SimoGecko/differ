#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <streambuf>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include "html_templates.h"

using namespace std;

using uint = unsigned int;
using lint = long long int;
using luint = long long unsigned int;

// repurposing unused ascii characters for special meaning
#define CHAR_ADD '\x1' // SOH (start of heading)
#define CHAR_REM '\x2' // STX (start of text)
#define CHAR_END '\x3' // ETX (end of text)

#define CHAR_ADDB '\x4' // EOT (end of transmission)
#define CHAR_REMB '\x5' // ENQ (enquiry)
#define CHAR_ENDB '\x6' // ACK (acknowledge)
#define CHAR_NEUB '\xE' // SO (shift out)

#define CHAR_FAUX '\xF' // SI (shift in)

enum class dtype      { none, same, rem, add, diff };
enum class bg         { clear, neut, add, rem };
enum class viewmode   { split, unified };
enum class diffmode   { character, word };
enum class diffmethod { lcs, patience, matchlines };
enum class dir        { none, up, left, diag };

// members
viewmode m_viewmode = viewmode::split;
diffmode m_diffmode = diffmode::character;
diffmethod m_diffmethod = diffmethod::patience;
bool ignorecase = false;
bool ignorewhitespace = false;
bool collapselines = true;
int contextsize = 3;
bool saveinputs = false;
const char* OUTPUTPATH = "c:\\dev\\differ\\"; // "%userprofile%\\AppData\\Local\\Temp\\";
bool converttabstospaces = true;
int spacesfortabscount = 4;

const char* filepath1 = nullptr;
const char* filepath2 = nullptr;

// TODO: organize code with well-defined names and index naming

struct slice
{
    int ia, ja; // [ia..ja[
    int ib, jb; // [ib..jb[
};

struct diffline
{
    dtype type;
    int liL, liR;
    string value, valueL, valueR;
    int numRem, numAdd;
    int dist;

    static diffline same(int liL, int liR, string valueL, string valueR) // might contain small differences
    {
        return diffline{ dtype::same, liL, liR, "", valueL, valueR, 0, 0, INT_MAX };
    }
    static diffline rem(int liL, string value)
    {
        return diffline{ dtype::rem, liL, -1, value, "", "", 1, 0, INT_MAX };
    }
    static diffline add(int liR, string value)
    {
        return diffline{ dtype::add, -1, liR, value, "", "", 0, 1, INT_MAX };
    }
    static diffline diff(int liL, int liR, string value, string valueL, string valueR, int numRem, int numAdd)
    {
        return diffline{ dtype::diff, liL, liR, value, valueL, valueR, numRem, numAdd, INT_MAX };
    }
};

struct corrdata // correspondencedata
{
    int countA, countB;
    int liA, liB; // first occurrence only
    corrdata() : countA(0), countB(0), liA(-1), liB(-1) {}
};

struct linedata
{
    //int li; // line index
    string value;
    unsigned int hash;
    int cli; // corresponding line index
};

void readparams(int argc, char** argv)
{
    //for (int i = 0; i < argc; ++i) cout << "argv[" << i << "]=" << argv[i] << "\n";

    filepath1 = argv[1];
    filepath2 = argv[2];

    for (int i = 3; i < argc; i++)
    {
        if      (_strcmpi(argv[i], "unifiedview")      == 0) m_viewmode = viewmode::unified;
        else if (_strcmpi(argv[i], "splitview")        == 0) m_viewmode = viewmode::split;
        else if (_strcmpi(argv[i], "ignorewhitespace") == 0) ignorewhitespace = true;
        else if (_strcmpi(argv[i], "ignorecase")       == 0) ignorecase = true;
        else if (_strcmpi(argv[i], "lcs")              == 0) m_diffmethod = diffmethod::lcs;
        else if (_strcmpi(argv[i], "patience")         == 0) m_diffmethod = diffmethod::patience;
        else if (_strcmpi(argv[i], "matchlines")       == 0) m_diffmethod = diffmethod::matchlines;
        else if (_strcmpi(argv[i], "collapselines")    == 0) collapselines = true;
        else if (_strcmpi(argv[i], "expandelines")     == 0) collapselines = false;
        else if (_strcmpi(argv[i], "saveinputs")       == 0) saveinputs = true;
        else if (_strcmpi(argv[i], "tabstospaces")     == 0) converttabstospaces = true;
        else if (_strcmpi(argv[i], "outputpath")       == 0 && (i + 1 < argc)) OUTPUTPATH = argv[++i];
        else if (_strcmpi(argv[i], "contextsize")      == 0 && (i + 1 < argc))
        {
            contextsize = std::stoi(argv[++i]);
        }
    }
}

//void readfilelines(const char* filepath, vector<string>& lines)
//{
//    std::ifstream file(filepath);
//    std::string line;
//    while (std::getline(file, line))
//    {
//        lines.push_back(line);
//    }
//}

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

int LCS(string& O, const string& A, const string& B) // naive but optimal first implementation
{
    int I = A.size();
    int J = B.size();
    vector<vector<int>> T(I+1, vector<int>(J+1, 0));
    for (int i = 1; i <= I; ++i)
    {
        for (int j = 1; j <= J; ++j)
        {
            int d = (A[i-1] == B[j-1]) ? 1 : 0;
            T[i][j] = max(max(T[i-1][j], T[i][j-1]), T[i-1][j-1] + d);
        }
    }
    int ans = T[I][J];
    // build-back
    int i = I, j = J;
    stringstream ss;
    dir mode = dir::none;

    auto adddelimiter = [&ss](dir mode, dir prevmode)
    {
        if (mode != prevmode)
        {
            // close previous
            if (prevmode == dir::left) ss << CHAR_ADD;
            else if (prevmode == dir::up) ss << CHAR_REM;

            // open new
            if (mode == dir::left || mode == dir::up) ss << CHAR_END;
        }
    };

    while (i > 0 && j > 0)
    {
        dir prevmode = mode;
        bool cangoUp   = T[i][j] == T[i-1][j];
        bool cangoLeft = T[i][j] == T[i][j-1];
        bool cangoDiag = A[i-1]==B[j-1];

        // LEFT=add, UP=del, DIAG=same

        // prefer to keep doing what it was doing before (up or left)
        if      (prevmode == dir::up   && cangoUp  ) mode = dir::up;
        else if (prevmode == dir::left && cangoLeft) mode = dir::left;
        else if (prevmode == dir::diag && cangoDiag) mode = dir::diag;
        else
        {
            mode = cangoLeft ? dir::left : (cangoUp ? dir::up : dir::diag);
        }

        adddelimiter(mode, prevmode);
        if      (mode == dir::up  )     ss << A[--i];
        else if (mode == dir::left)     ss << B[--j];
        else  /*(mode == dir::diag)*/ { ss << A[--i]; --j; }
    }
    // do the final stretch
    if (i > 0)
    {
        adddelimiter(dir::up, mode);
        while (i > 0) ss << A[--i];
        adddelimiter(dir::none, dir::up);
    }
    else if (j > 0)
    {
        adddelimiter(dir::left, mode);
        while (j > 0) ss << B[--j];
        adddelimiter(dir::none, dir::left);
    }

    O = ss.str();
    reverse(O.begin(), O.end());
    return ans;
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

int removeall(string& s, char c) // returns the number or removals
{
    size_t size = s.size();
    s.erase(remove(s.begin(), s.end(), c), s.end());
    return size - s.size();
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

void removeallreplaceinrange(string& str, char from, char to, char ex, char faux)
{
    // remove all occurrences of characters [from..to] while keeping the ex in range and converting them to faux
    size_t start_pos = 0, end_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != string::npos)
    {
        if ((end_pos = str.find(to, start_pos)) != string::npos)
        {
            // count how many ex appear
            int count = 0;
            for (int i = start_pos; i <= end_pos; ++i)
            {
                if (str[i] == ex) ++count;
            }
            str.erase(start_pos, end_pos - start_pos + 1);

            str.insert(start_pos, count, faux);
            start_pos += count;
        }
        else
        {
            return;
        }
    }
}

int count(const string& s, char c)
{
    return std::count(s.begin(), s.end(), c);
}

int countatbeginning(const string& s, char c)
{
    int i = 0;
    while (i < s.size() && s[i] == c)++i;
    return i;
}

void htmlifly(string& s) // PERF: single pass
{
    replaceall(s, '<', "&lt;");
    replaceall(s, '>', "&gt;");
    replaceall(s, '\n', "<br>");
    //replaceall(s, "    ", "&emsp;");
    //replaceall(s, "  ", "&ensp;");
    if (converttabstospaces)
    {
        replaceall(s, '\t', "    ");
    }
    replaceall(s, ' ', "&nbsp;"); // only leading ones
}

string gethtmltemplate()
{
    string ans = "";
    if (m_viewmode == viewmode::split)
    {
        // TODO: #if embedded
        //readfile("x:/Vs/Differ/template_split.html", ans);
        ans = HTML_templatesplit;
        replaceone(ans, "__STYLE__", HTML_style);
    }
    else if (m_viewmode == viewmode::unified)
    {
        //readfile("x:/Vs/Differ/template_unified.html", ans);
        ans = HTML_templateunified;
        replaceone(ans, "__STYLE__", HTML_style);
    }
    return ans;
}

void converttags(string& s) // PERF: single pass
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
    htmlifly(s);

    if (m_viewmode == viewmode::split)
    {
        // these add the infamous grey empty lines (not sure why)
        //replaceall(s, "\1\n", "\n\1");
        //replaceall(s, "\2\n", "\n\2");
        //replaceall(s, "\n\3", "\3\n");

        string sl = s;
        string sr = s;

        //replaceall(sl, CHAR_ADD, "\14 \6\1");
        //replaceall(sr, CHAR_REM, "\14 \6\2");
        removeall(sl, CHAR_ADD, CHAR_END);
        removeall(sr, CHAR_REM, CHAR_END);

        // iterate over lines, add backgrounds, create lines
        htmlifly(sl);
        htmlifly(sr);

        converttags(sl);
        converttags(sr);

        string html = gethtmltemplate();
        replaceone(html, "__LINE_R__", "_"); // in reverse order for speed
        replaceone(html, "__LINE_L__", "_");
        replaceone(html, "__CODE_R__", sr);
        replaceone(html, "__CODE_L__", sl);
        return html;
    }
    else if (m_viewmode == viewmode::unified)
    {
        //replaceall(s, "\1\n", "\n\1");
        //replaceall(s, "\2\n", "\n\2");
        //replaceall(s, "\n\3", "\3\n");
        
        
        //std::stringstream ssi(s);
        //std::string line;

        //stringstream sso;
        //while (std::getline(ssi, line, '\n'))
        //{
        //    if (contains(line, CHAR_END))
        //    {
        //        sso << CHAR_NEUB << line << CHAR_ENDB << "\n";
        //    }
        //    else
        //    {
        //        sso << line << "\n";
        //    }
        //}
        //s = sso.str(); // output

        htmlifly(s);
        converttags(s);

        string html = gethtmltemplate();
        replaceone(html, "__LINE_R__", "_"); // in reverse order for speed
        replaceone(html, "__LINE_L__", "_");
        replaceone(html, "__CODE__", s);
        return html;
    }

    return "";
}

string convert2html(const vector<diffline>& D)
{
    // add here all the things
    if (m_viewmode == viewmode::split)
    {
        stringstream ssl, ssr, sll, slr;

        bg bg1 = bg::clear, bg2 = bg::clear;
        auto boundary = [](bg& prev, bg curr, stringstream& sss)
        {
            if (prev != curr)
            {
                if (prev != bg::clear) sss << CHAR_ENDB;
                prev = curr;
                if      (curr == bg::neut) sss << CHAR_NEUB;
                else if (curr == bg::add ) sss << CHAR_ADDB;
                else if (curr == bg::rem ) sss << CHAR_REMB;
            }
        };

        bool skipped = true;
        for (const diffline& d : D)
        {
            // check skipped
            if (collapselines && d.dist > contextsize) // skip
            {
                if (!skipped)
                {
                    skipped = true;
                    // add border
                    string spacing(5, '\n');
                    ssl << spacing;
                    ssr << spacing;
                    sll << spacing;
                    slr << spacing;
                }
                continue;
            }
            skipped = false;

            // check for backgrounds
            switch (d.type)
            {
            case dtype::same:
            {
                boundary(bg1, bg::clear, ssl);
                boundary(bg2, bg::clear, ssr);
                ssl << d.valueL << '\n';
                ssr << d.valueR << '\n';
            }
            break;
            case dtype::rem:
            {
                boundary(bg1, bg::rem, ssl);
                boundary(bg2, bg::neut, ssr);
                ssl << d.value << '\n';
                ssr << '\n';
            }
            break;
            case dtype::add:
            {
                boundary(bg1, bg::neut, ssl);
                boundary(bg2, bg::add, ssr);
                ssl << '\n';
                ssr << d.value << '\n';
            }
            break;
            case dtype::diff:
            {
                boundary(bg1, d.liL>-1? bg::rem : bg::neut, ssl);
                boundary(bg2, d.liR>-1? bg::add : bg::neut, ssr);
                ssl << d.valueL << '\n';
                ssr << d.valueR << '\n';
            }
            break;
            }
            if (d.liL > -1) sll << d.liL + 1;
            if (d.liR > -1) slr << d.liR + 1;
            sll << '\n';
            slr << '\n';
        }

        string sl = ssl.str();
        string sr = ssr.str();
        string ll = sll.str();
        string lr = slr.str();

        htmlifly(sl);
        htmlifly(sr);
        htmlifly(ll);
        htmlifly(lr);

        converttags(sl);
        converttags(sr);

        string html = gethtmltemplate();
        replaceone(html, "__LINE_R__", lr); // in reverse order for speed
        replaceone(html, "__LINE_L__", ll);
        replaceone(html, "__CODE_R__", sr);
        replaceone(html, "__CODE_L__", sl);
        return html;
    }
    else if (m_viewmode == viewmode::unified)
    {
        /*
        stringstream ss, sll, slr;
        bool skipped = true;
        for (const diffline& d : D)
        {
            // check skipped
            if (collapselines && d.dist > contextsize) // skip
            {
                if (!skipped)
                {
                    skipped = true;
                    // add border
                    string spacing(5, '\n');
                    ss << spacing;
                    sll << spacing;
                    slr << spacing;
                }
                continue;
            }
            skipped = false;

            // check for backgrounds

            switch (d.t)
            {
            case dtype::same:
            {
                ss << d.value << '\n';
            }
            break;
            case dtype::rem:
            {
                ss << CHAR_REMB << d.value << '\n' << CHAR_ENDB;
            }
            break;
            case dtype::add:
            {
                ss << CHAR_ADDB << d.value << '\n' << CHAR_ENDB;
            }
            break;
            case dtype::diff:
            {
                ss << CHAR_REMB << d.valueL << '\n' << CHAR_ENDB;
                ss << CHAR_ADDB << d.valueR << '\n' << CHAR_ENDB;
            }
            break;
            }
            if (d.liL > -1) sll << d.liL + 1; sll << '\n';
            if (d.liR > -1) slr << d.liR + 1; slr << '\n';
        }

        string s = ss.str();
        string ll = sll.str();
        string lr = slr.str();

        htmlifly(s);
        converttags(s);

        string html = gethtml();
        //readfile("x:/Vs/Differ/template_unified.html", html);
        replaceone(html, "__LINE_R__", "_"); // in reverse order for speed
        replaceone(html, "__LINE_L__", "_");
        replaceone(html, "__CODE__", s);
        return html;
        */
    }
    return "";
}

uint computehash(const string& s)
{
    // h = (sum_{i=0}^{n-1} s[i]*p^i) % m
    // p =~ alphabet size
    // m = modulo

    const int p = 97;
    const int m = (int)(1e9) + 9;

    luint hash_value = 0;
    luint p_pow = 1;

    bool isspace=false, wasspace=false, isuppercase=false;

    for (char c : s) // PERF: move checks outside for better performance, or use multiple functions
    {
        if (ignorewhitespace)
        {
            if ('\t' <= c && c <= '\r') c = ' '; // whitespace = \t\n\v\f\r \u00A0\u2028\u2029
            isspace = (c == ' '); // || c == '\t');
            if (isspace && wasspace) continue;
            wasspace = isspace;
        }

        if (ignorecase)
        {
            isuppercase = ('A' <= c && c <= 'Z');
            if (isuppercase) c += 'a' - 'A';
        }

        hash_value = (hash_value + (c - ' ' + 1) * p_pow) % m;
        p_pow = (p_pow * p) % m;
    }
    return (uint)hash_value;
}

void readlinedata(vector<linedata>& ans, const char* filepath)
{
    ans.clear();
    std::ifstream file(filepath);
    std::string line;
    //int ln = 0;
    while (std::getline(file, line))
    {
        // PERF: both at the same time
        //unsigned int hash = gethash(line);
        ans.push_back({/*ln,*/ line, /*hash*/ 0u, -1 });
        //++ln;
        //if (file.eof()) break;
    }
    if (file.eof() && file.fail())
    {
        ans.push_back({ "", 0u, -1 }); // last empty line
    }
}

void hashlinedata(vector<linedata>& lds)
{
    for (linedata& ld : lds)
    {
        unsigned int hash = computehash(ld.value);
        ld.hash = hash;
    }
}

vector<int> LIS(const vector<int>& A) // from https://cp-algorithms.com/sequences/longest_increasing_subsequence.html
{
    if (A.size() < 2) return A;

    int N = A.size();
    vector<int> d(N, 1), p(N, -1);
    for (int i = 0; i < N; i++)
    {
        for (int j = 0; j < i; j++)
        {
            if (A[j] < A[i] && d[i] < d[j] + 1)
            {
                d[i] = d[j] + 1;
                p[i] = j;
            }
        }
    }

    int ans = d[0], pos = 0;
    for (int i = 1; i < N; i++)
    {
        if (d[i] > ans)
        {
            ans = d[i];
            pos = i;
        }
    }

    vector<int> subseq;
    while (pos != -1)
    {
        subseq.push_back(A[pos]);
        pos = p[pos];
    }
    reverse(subseq.begin(), subseq.end());
    return subseq;
}

// TODO: this doesn't work for example for {3,3,3,2} -> fix
vector<int> LISold(const vector<int>& A) // (we don't have duplicates)
{
    if (A.size() < 2) return A;

    vector<int> S; // stack top indices
    vector<int> P(A.size()); // pointers
    for (int i = 0; i < (int)A.size(); ++i)
    {
        // find leftmost stack index whose uppermost value is greater than the value
        // PERF: binsearch
        int s = -1;
        for (int j = 0; j < (int)S.size(); ++j)
        {
            if (A[S[j]] > A[i])
            {
                s = j; break;
            }
        }
        if (s != -1) // found
        {
            P[i] = s > 0 ? S[s - 1] : -1;
            S[s] = i;
        }
        else
        {
            P[i] = S.empty() ? -1 : S[S.size() - 1];
            S.push_back(i);
        }
    }
    // reconstruct
    vector<int> ans;
    int j = S[S.size() - 1];
    while (j != -1)
    {
        ans.push_back(A[j]);
        j = P[j];
    }
    reverse(ans.begin(), ans.end());
    return ans;
}

void findallcorrespondences(vector<linedata>& A, vector<linedata>& B)
{
    bool atleastonecorr = false;
    auto addcorr = [&](int liA, int liB)
    {
        A[liA].cli = liB;
        B[liB].cli = liA;
        //cout << "corr ln:" << liA + 1 << "-" << liB + 1 << endl;
        atleastonecorr = true;
    };

    queue<slice> Q;
    Q.push({ 0, (int)A.size(), 0, (int)B.size() });

    while (!Q.empty())
    {
        slice s = Q.front(); Q.pop();
        //cout << "slice ln: [" << s.ia+1 << "," << s.ja << "] [" << s.ib+1 << "," << s.jb << "]\n";
        int ia = s.ia; int ja = s.ja;
        int ib = s.ib; int jb = s.jb;

        atleastonecorr = false;

        if (ia >= ja || ib >= jb) continue; // no lines to parse

        // match up the same lines at the beginning
        while (ia < ja && ib < jb && A[ia].hash == B[ib].hash)
        {
            addcorr(ia, ib);
            ++ia; ++ib;
        }
        // match up the same lines at the end
        while (ja > ia && jb > ib && A[ja - 1].hash == B[jb - 1].hash)
        {
            --ja; --jb;
            addcorr(ja, jb);
        }

        if (ia >= ja || ib >= jb) continue; // no lines to parse

        // find unique correspondences between lines
        unordered_map<uint, corrdata> M;

        for (int a = ia; a < ja; ++a)
        {
            linedata& l = A[a];
            corrdata& c = M[l.hash]; // insert or find. default ctor
            ++c.countA;
            if (c.countA == 1) c.liA = a;
        }
        for (int b = ib; b < jb; ++b)
        {
            linedata& l = B[b];
            corrdata& c = M[l.hash]; // insert or find. default ctor
            ++c.countB;
            if (c.countB == 1) c.liB = b;
        }

        // PERF: only use vC
        map<int, int> Mba; // map liB->liA
        vector<pair<int, int>> vC; // possible correspondences
        for (const auto& p : M)
        {
            const corrdata& c = p.second;
            if (c.countA == 1 && c.countB == 1)
            {
                Mba[c.liB] = c.liA;
                vC.push_back({ c.liA, c.liB });
            }
        }

        // sort pairs according to lnA
        sort(vC.begin(), vC.end());
        // find LIS in lnB (also prunes those who don't appear in the LIS)
        vector<int> lisB(vC.size());
        for (int i = 0; i < (int)vC.size(); ++i) lisB[i] = vC[i].second;
        lisB = LIS(lisB);

        // assign correspondences
        for (int liB : lisB)
        {
            int liA = Mba[liB];
            addcorr(liA, liB);
        }

        if (atleastonecorr) // something changed. prepare subblocks
        {
            // find blocks to iterate over again, call recursively
            int sa = ia;
            while (sa < ja)
            {
                // find first unmatched
                while (sa < ja && A[sa].cli != -1) ++sa;
                if (sa >= ja) break; // not found

                int ea = sa + 1;
                // find one after last unmatched
                while (ea < ja && A[ea].cli == -1) ++ea;

                // find corresponding interval
                int sb = sa-1 >= ia ? A[sa-1].cli+1 : ib;
                int eb = ea   <  ja ? A[ea  ].cli   : jb;

                slice sn{ sa, ea, sb, eb };
                //cout << "add li: [" << sn.ia+1 << "," << sn.ja << "] [" << sn.ib+1 << "," << sn.jb << "]\n";
                Q.push(sn);

                sa = ea + 1;
            }
        }
    }
}

void splitstring(const string& s, vector<string>& a, char delimiter)
{
    size_t last = 0;
    size_t next = 0;
    while ((next = s.find(delimiter, last)) != string::npos)
    {
        a.push_back(s.substr(last, next - last));
        last = next + 1;
    }
    a.push_back(s.substr(last));
}

void patience(vector<diffline>& D, const vector<linedata>& A, const vector<linedata>& B)
{
    // iterate over the lines
    //   if they have correspondences -> add them to the result
    //   if not -> run LCS on them
    
    int ia = 0, ib = 0;
    while (ia < A.size() && ib < B.size())
    {
        if (A[ia].cli == ib) // correspondence
        {
            assert(B[ib].cli == ia && "mismatch in correspondence");
            D.push_back(diffline::same(ia, ib, A[ia].value, B[ib].value));
            ++ia; ++ib;
        }
        else if (B[ib].cli != -1) // one-sided REM
        {
            int sa = ia, ea = B[ib].cli;
            for (; ia < ea; ++ia)
            {
                D.push_back(diffline::rem(ia, A[ia].value));
            }
        }
        else if (A[ia].cli != -1) // one-sided ADD
        {
            int sb = ib, eb = A[ia].cli;
            for (; ib < eb; ++ib)
            {
                D.push_back(diffline::add(ib, B[ib].value));
            }
        }
        else // MIX
        {
            // build the pieces to diff
            stringstream piecesA, piecesB;
            int sa = ia, sb = ib;
            while (ia < A.size() && A[ia].cli == -1)
            {
                piecesA << A[ia].value << '\n';
                ++ia;
            }
            while (ib < B.size() && B[ib].cli == -1)
            {
                piecesB << B[ib].value << '\n';
                ++ib;
            }
            int ea = ia, eb = ib;

            //printf("diff [%d,%d], [%d,%d]\n", sa, ea - 1, sb, eb - 1);

            // remove last '\n'
            string pieceA = piecesA.str(); if (pieceA.size() < 1) continue; pieceA.pop_back();
            string pieceB = piecesB.str(); if (pieceB.size() < 1) continue; pieceB.pop_back();

            // run LCS
            string diffu;
            int lcsCount = LCS(diffu, pieceA, pieceB); // the lcs contains \n for both lines
            //printf("lcs: %d\t%d\t%d\n", pieceA.size(), pieceB.size(), lcsCount);
            string diffl = diffu;
            string diffr = diffu;

            //removeall(diffl, CHAR_ADD, CHAR_END);
            //removeall(diffr, CHAR_REM, CHAR_END);
            
            // the faux characters signal desire to have a new line
            removeallreplaceinrange(diffl, CHAR_ADD, CHAR_END, '\n', CHAR_FAUX);
            removeallreplaceinrange(diffr, CHAR_REM, CHAR_END, '\n', CHAR_FAUX);

            // naive first implementation: split and put all at the beginning
            int da = ea - sa, db = eb - sb; // delta (number of lines)

            // TODO (maybe resolved): there are still some issues to iron out here
            vector<string> vu, va, vb;
            splitstring(diffu, vu, '\n');
            splitstring(diffl, va, '\n');
            splitstring(diffr, vb, '\n');
            int fauxl = countatbeginning(va[0], CHAR_FAUX);
            int fauxr = countatbeginning(vb[0], CHAR_FAUX);
            if (fauxl > 0) va[0] = va[0].substr(fauxl);
            if (fauxr > 0) vb[0] = vb[0].substr(fauxr);
            int xxL = 0, xxR = 0;
            int imax = count(diffu, '\n')+1;
            for (int i=0; i < imax; ++i) // was min
            {
                int numRem = -1, numAdd = -1; // TODO: compute

                string lineL = "";
                string lineR = "";
                int lineiL = -1;
                int lineiR = -1;

                if (fauxl == 0) // read next
                {
                    lineL = va[xxL];
                    lineiL = sa+xxL;
                    ++xxL;
                    fauxl = removeall(lineL, CHAR_FAUX);
                }
                else --fauxl;
                if (fauxr == 0)
                {
                    lineR = vb[xxR];
                    lineiR = sb+xxR;
                    ++xxR;
                    fauxr = removeall(lineR, CHAR_FAUX);
                }
                else --fauxr;
                //if (lineL[0] == CHAR_FAUX) lineL = lineL.substr(1); else lineiL = sa2++;
                //if (lineR[0] == CHAR_FAUX) lineR = lineR.substr(1); else lineiR = sb2++;

                D.push_back(diffline::diff(lineiL, lineiR, "", lineL, lineR, numRem, numAdd));
            }
        }
    }
    // run final segment
    while (ia < A.size()) // one-sided REM
    {
        //D.push_back(diffline::rem(ia, A[ia].value)); ++ia;
    }
    while (ib < B.size()) // one-sided ADD
    {
        //D.push_back(diffline::add(ib, B[ib].value)); ++ib;
    }
}

string trimleadingwhitespace(const std::string& s)
{
    const std::string WHITESPACE = " \n\r\t\f\v";
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

int computesimilarity(const string& a, const string& b)
{
    //string A = ignorewhitespace ? trimleadingwhitespace(a) : a;
    //string B = ignorewhitespace ? trimleadingwhitespace(b) : b;
    string o;
    return LCS(o, a, b);
}

int thresholdsimilarity(const string& a, const string& b)
{
    // at least 30% of the minimum of two strings?
    float matchPercent = 0.5f;
	//int ans = (int)((float)min(a.size(), b.size()) * matchPercent);
	int ans = (int)(((float)(a.size() + b.size()))/2.f * matchPercent);
    return ans;
}

void matchlines(vector<diffline>& D, const vector<linedata>& A, const vector<linedata>& B)
{
    // NOTES: the first 3 cases are the same as patience. only the mix case first matches lines by using similarty metric and then runs lcs on them
    
    // iterate over the lines
    //   if they have correspondences -> add them to the result
    //   if not -> run LCS on them
    
    int ia = 0, ib = 0;
    while (ia < A.size() && ib < B.size())
    {
        if (A[ia].cli == ib) // correspondence
        {
            assert(B[ib].cli == ia && "mismatch in correspondence");
            D.push_back(diffline::same(ia, ib, A[ia].value, B[ib].value));
            ++ia; ++ib;
        }
        else if (B[ib].cli != -1) // one-sided REM
        {
            int sa = ia, ea = B[ib].cli;
            for (; ia < ea; ++ia)
            {
                D.push_back(diffline::rem(ia, A[ia].value));
            }
        }
        else if (A[ia].cli != -1) // one-sided ADD
        {
            int sb = ib, eb = A[ia].cli;
            for (; ib < eb; ++ib)
            {
                D.push_back(diffline::add(ib, B[ib].value));
            }
        }
        else // MIX
        {
            // find the range of mixed lines
            int sa = ia, sb = ib;
            while (ia < A.size() && A[ia].cli == -1) ++ia;
            while (ib < B.size() && B[ib].cli == -1) ++ib;
            int ea = ia, eb = ib;

            int na = ea - sa; // num lines
            int nb = eb - sb;

            struct want
            {
                int strenght;
                int ia;
                int ib;
            };

            // find possible candidates
            //vector<int> C(nb, -1); // which line wants it
            //vector<int> S(nb, 0); // with which strength he want is
            vector<want> wants;
            for (int ia=sa; ia<ea; ++ia)
            {
                //int bestMatch = -1;
                //int maxSimilarity = 0;
                // find the best candidate for matching line, above some threshold
                for (int ib = sb; ib < eb; ++ib)
                {
                    int similarity = computesimilarity(A[ia].value, B[ib].value);
                    bool passesThreshold = similarity > 0 && similarity >= thresholdsimilarity(A[ia].value, B[ib].value);
                    if (passesThreshold)// && similarity > S[ib-sb])
                    {
                        wants.push_back({ similarity, ia, ib });
                        //C[ib-sb] = ia;
                        //S[ib-sb] = similarity;
                        //maxSimilarity = similarity;
                        //bestMatch = ib;
                    }
                }
                //C[ia-sa] = bestMatch;
            }
            sort(wants.begin(), wants.end(), [](const want& wa, const want& wb) { return wa.strenght > wb.strenght; });
            vector<int> ma(na, -1), mb(nb, -1);
            for (want& w : wants)
            {
                if (ma[w.ia-sa] == -1 && mb[w.ib-sb] == -1) // both untaken
                {
                    ma[w.ia-sa]=w.ib;
                    mb[w.ib-sb]=w.ia;
                }
            }
            vector<int> Ca(ma); // correspondences a
            Ca.erase(remove(Ca.begin(), Ca.end(), -1), Ca.end());
            vector<int> lisCa = LIS(Ca); // find the most matches
            int im = 0;
            for (int i = 0; i < nb; ++i) mb[i] = -1; // reset
            for (int i = 0; i < na; ++i)
            {
                if (im < lisCa.size() && ma[i] == lisCa[im])
			{
                    mb[ma[i]-sb] = sa+i;
                    ++im;
                }
                else ma[i] = -1; // set invalid
			}


            // iterate, add lines one by one running diff on those that match
            int ia = sa, ib = sb;//, im = 0;
            //int nm = lisC.size();
            while (ia < ea || ib < eb)
            {
                // if im<nm we can assume ia<ea && ib<eb
                //if (im<nm && C[ia-sa] == lisC[im] && C[ia-sa] == ib) // matching line
                //{
                //    int numRem = -1, numAdd = -1; // TODO: compute
                //    D.push_back(diffline::diff(ia, ib, "", A[ia].value, B[ib].value, numRem, numAdd));
                //    ++ia; ++ib; ++im;
                //}
                if (ia<ea && ma[ia-sa] == -1) // waiting for A
                {
                    stringstream ss; ss << CHAR_REM << A[ia].value << CHAR_END;
                    D.push_back(diffline::rem(ia, ss.str()));
                    ++ia;
                }
                else if (ib<eb && mb[ib-sb] == -1) // waiting for B
                {
                    stringstream ss; ss << CHAR_ADD << B[ib].value << CHAR_END;
                    D.push_back(diffline::add(ib, ss.str()));
                    ++ib;
                }
                else
                {
                    // both non -1, must be matching
					int numRem = -1, numAdd = -1; // TODO: compute
                    // TODO: do LCS, don't use raw lines values
                    string o;
                    LCS(o, A[ia].value, B[ib].value);
                    string sl(o), sr(o);
                    removeall(sl, CHAR_ADD, CHAR_END);
                    removeall(sr, CHAR_REM, CHAR_END);
					D.push_back(diffline::diff(ia, ib, o, sl, sr, numRem, numAdd));
					++ia; ++ib;
                }
            }
        }
    }
    // run final segment (necesssary?)
    while (ia < A.size()) // one-sided REM
    {
        //D.push_back(diffline::rem(ia, A[ia].value)); ++ia;
    }
    while (ib < B.size()) // one-sided ADD
    {
        //D.push_back(diffline::add(ib, B[ib].value)); ++ib;
    }
}

void computedistance(vector<diffline>& D)
{
    // compute distance in double pass
    int dist = INT_MAX;

    auto assigndist = [&D, &dist](int i)
    {
        if (D[i].type != dtype::same) dist = 0;
        else if (dist < INT_MAX) ++dist;
        D[i].dist = min(D[i].dist, dist);
    };

    dist = INT_MAX;
    for (int i = 0; i < D.size();  ++i) assigndist(i);
    dist = INT_MAX;
    for (int i = D.size() - 1; i >= 0; --i) assigndist(i);
}

int main(int argc, char** argv)
{
    cout << "Differ - (c) 2022 Simone Guggiari" << endl << endl;

    if (argc < 3)
    {
        cout << "Too few parameters. Usage: differ.exe file1.txt file2.txt [options]";
        return 1;
    }


    // test LIS
    //vector<int> cards({ 9, 4, 6, 12, 8, 7, 1, 5, 10, 11, 3, 2, 13 });
    //vector<int> ans = LIS(cards); // LIS(cards) = 4 6 7 10 11 13

    readparams(argc, argv);
    
    if (filepath1 == nullptr || filepath2 == nullptr) // TODO: check the files exist, can be opened
    {
        return 1;
    }

    auto starttime = chrono::high_resolution_clock::now();
    auto TIMER_END = [&starttime](const string& msg)
    {
        auto endtime = chrono::high_resolution_clock::now();
        chrono::duration<double, milli>float_ms = endtime - starttime;
        cout << msg << " in " << float_ms.count() << " ms" << endl;
        starttime = endtime;
    };

    string resulthtml;

    // save to file
    if(saveinputs)
    {
        string file1, file2;
        readfile(filepath1, file1);
        readfile(filepath2, file2);
        string outputfilepath1 = string(OUTPUTPATH) + "output_lastfile1.txt";
        string outputfilepath2 = string(OUTPUTPATH) + "output_lastfile2.txt";

        writefile(outputfilepath1.c_str(), file1);
        writefile(outputfilepath2.c_str(), file2);
    }

    // TODO: move here the reading of file lines

    if (m_diffmethod == diffmethod::lcs)
    {
        string file1, file2;
        readfile(filepath1, file1);
        readfile(filepath2, file2);
        TIMER_END("read");

        string diffresult;
        LCS(diffresult, file1, file2);
        TIMER_END("diff");

        string resulthtml = convert2html(diffresult);
        TIMER_END("html");
    }
    else if (m_diffmethod == diffmethod::patience || m_diffmethod == diffmethod::matchlines)
    {
        vector<linedata> A, B;
        readlinedata(A, filepath1);
        readlinedata(B, filepath2);
        TIMER_END("read");

        hashlinedata(A);
        hashlinedata(B);
        TIMER_END("hash");

        findallcorrespondences(A, B);
        TIMER_END("corr");

        vector<diffline> D;
        if (m_diffmethod == diffmethod::patience)
        {
            patience(D, A, B);
        }
        else
        {
            matchlines(D, A, B);
        }
        computedistance(D);
        TIMER_END("pati");

        resulthtml = convert2html(D);
        TIMER_END("html");
    }

    string outputfilepath = string(OUTPUTPATH) + "differresult.html";
    // TODO: resolve the path, make sure it can be written. use a default path if not
    writefile(outputfilepath.c_str(), resulthtml);
    TIMER_END("writ");

    ShellExecuteA(NULL, "open", outputfilepath.c_str(), NULL, NULL, SW_SHOWNORMAL);
    TIMER_END("open");

    //system("PAUSE");
    return 0;
}