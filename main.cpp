#include <algorithm>
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

using namespace std;

//#define contains(c,x) ((c).find(x) != (c).end())

#define VIEWMODE_SPLIT 1
#define VIEWMODE_UNIFIED 2
//#define DIFFMODE_CHARACTER 1
//#define DIFFMODE_WORD 2

#define DIFFMETHOD_LCS 1
#define DIFFMETHOD_PATIENCE 2

// members
int viewmode = VIEWMODE_UNIFIED;
//int diffmode = DIFFMODE_CHARACTER;
int diffmethod = DIFFMETHOD_LCS;
bool ignorecase = false;
bool ignorewhitespace = false;
bool collapselines = false;
int contextsize = 3;

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

using uint = unsigned int;
using lint = long long int;
using luint = long long unsigned int;

void readparams(int argc, char** argv)
{
    filepath1 = argv[1];
    filepath2 = argv[2];

	for (int i = 3; i < argc; i++)
    {
        if      (_strcmpi(argv[i], "unifiedview")      == 0) viewmode = VIEWMODE_UNIFIED;
        else if (_strcmpi(argv[i], "splitview")        == 0) viewmode = VIEWMODE_SPLIT;
        else if (_strcmpi(argv[i], "ignorewhitespace") == 0) ignorewhitespace = true;
        else if (_strcmpi(argv[i], "ignorecase")       == 0) ignorecase = true;
        else if (_strcmpi(argv[i], "lcs")              == 0) diffmethod = DIFFMETHOD_LCS;
        else if (_strcmpi(argv[i], "patience")         == 0) diffmethod = DIFFMETHOD_PATIENCE;
        else if (_strcmpi(argv[i], "collapselines")    == 0) collapselines = true;
        else if (_strcmpi(argv[i], "expandelines")     == 0) collapselines = false;
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

#define NONE 0
#define UP 1
#define LEFT 2
#define DIAG 4

string LCS(const string& A, const string& B) // naive but optimal first implementation
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
    // build-back
    int i = I, j = J;
    stringstream ss;
    int mode = NONE;

    auto adddelimiter = [&ss](int mode, int prevmode)
    {
        if (mode != prevmode)
        {
            // close previous
            if (prevmode == LEFT) ss << CHAR_ADD;
            else if (prevmode == UP) ss << CHAR_REM;

            // open new
            if (mode == LEFT || mode == UP) ss << CHAR_END;
        }
    };

    while (i > 0 && j > 0)
    {
        int prevmode = mode;
        bool cangoUp   = T[i][j] == T[i-1][j];
        bool cangoLeft = T[i][j] == T[i][j-1];
        bool cangoDiag = A[i-1]==B[j-1];

        // LEFT=add, UP=del, DIAG=same

        // prefer to keep doing what it was doing before (up or left)
        if      (prevmode == UP   && cangoUp  ) mode = UP;
        else if (prevmode == LEFT && cangoLeft) mode = LEFT;
        else if (prevmode == DIAG && cangoDiag) mode = DIAG;
        else
        {
            mode = cangoLeft ? LEFT : (cangoUp ? UP : DIAG);
            //mode = cangoDiag ? DIAG : (cangoLeft ? LEFT : UP);
        }

        adddelimiter(mode, prevmode);
		if      (mode == UP  )     ss << A[--i];
        else if (mode == LEFT)     ss << B[--j];
        else  /*(mode == DIAG)*/ { ss << A[--i]; --j; }
    }
    // do the final stretch
    if (i > 0)
    {
        adddelimiter(UP, mode);
        while (i > 0) ss << A[--i];
        adddelimiter(NONE, UP);
    }
    else if (j > 0)
    {
        adddelimiter(LEFT, mode);
        while (j > 0) ss << B[--j];
        adddelimiter(NONE, LEFT);
    }

    // reverse it
    string ans = ss.str();
    reverse(ans.begin(), ans.end());
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

void removeallexcept(string& str, char from, char to, char ex) // faux
{
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
            //str.insert(start_pos, count, ex);
            //start_pos += count;
            for (int i = 0; i < count; i++)
            {
                str.insert(start_pos++, 1, ex);
                str.insert(start_pos++, 1, 'F'); // faux
            }
        }
        else
        {
            return;
        }
    }
}

void htmlifly(string& s) // TODO: single pass
{
    replaceall(s, '<', "&lt;");
    replaceall(s, '>', "&gt;");
    //replace(s, '\n', "<br>");
    //replace(s, "    ", "&emsp;");
    //replace(s, "  ", "&ensp;");
    //replace(s, " ", "&nbsp;"); // only leading ones
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

string convert2html_lcs(string& s)
{
    htmlifly(s);

    if (viewmode == VIEWMODE_SPLIT)
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
        converttags(sl);
        converttags(sr);



        string html;
        readfile("x:/Vs/Differ/template_split.html", html);
        replaceone(html, "__LINE_R__", "_"); // in reverse order for speed
        replaceone(html, "__LINE_L__", "_");
        replaceone(html, "__CODE_R__", sr);
        replaceone(html, "__CODE_L__", sl);
        return html;
    }
    else if (viewmode == VIEWMODE_UNIFIED)
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

        converttags(s);

        string html;
        readfile("x:/Vs/Differ/template_unified.html", html);
        replaceone(html, "__LINE_R__", "_"); // in reverse order for speed
        replaceone(html, "__LINE_L__", "_");
        replaceone(html, "__CODE__", s);
        return html;
    }

    return "";
}

struct diffline
{
    enum class type { same, rem, add, diff };

    type t;
    int liL, liR;
    string value, valueL, valueR;
    int numRem, numAdd;
    int dist;

    static diffline same(int liL, int liR, string value)
    {
        return diffline(type::same, liL, liR, value, "", "", 0, 0, INT_MAX);
    }
    static diffline rem(int liL, string value)
    {
        return diffline(type::rem, liL, -1, value, "", "", 1, 0, INT_MAX);
    }
    static diffline add(int liR, string value)
    {
        return diffline(type::add, -1, liR, value, "", "", 0, 1, INT_MAX);
    }
    static diffline diff(int liL, int liR, string value, string valueL, string valueR, int numRem,
        int numAdd)
    {
        return diffline(type::diff, liL, liR, value, valueL, valueR, numRem, numAdd, INT_MAX);
    }
    diffline(type _t, int _liL, int _liR, string _value, string _valueL, string _valueR,
        int _numRem, int _numAdd, int _dist)
        : t(_t), liL(_liL), liR(_liR), value(_value), valueL(_valueL), valueR(_valueR)
        , numRem(_numRem), numAdd(_numAdd), dist(_dist)
    {
    }
};


string convert2html_patience(const vector<diffline>& D)
{
    // add here all the things


    if (viewmode == VIEWMODE_SPLIT)
    {
        stringstream ssl, ssr, sll, slr;
        bool skipped = true;
        for (const diffline& d : D)
        {
            switch (d.t)
            {
            case diffline::type::same:
            {
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
                ssl << d.value << '\n';
                ssr << d.value << '\n';
            }
            break;
            case diffline::type::rem: // TODO: add delimeters only if different from previous
            {
                ssl << CHAR_REMB << d.value << '\n' << CHAR_ENDB;
                ssr << CHAR_NEUB << '\n' << CHAR_ENDB;
            }
            break;
            case diffline::type::add:
            {
                ssl << CHAR_NEUB << '\n' << CHAR_ENDB;
                ssr << CHAR_ADDB << d.value << '\n' << CHAR_ENDB;
            }
            break;
            case diffline::type::diff:
            {
                ssl << CHAR_REMB << d.valueL << '\n' << CHAR_ENDB;
                ssr << CHAR_ADDB << d.valueR << '\n' << CHAR_ENDB;
            }
            break;
            }
            if (d.liL > -1) sll << d.liL + 1; sll << '\n';
            if (d.liR > -1) slr << d.liR + 1; slr << '\n';
        }

        string sl = ssl.str();
        string sr = ssr.str();
        string ll = sll.str();
        string lr = slr.str();

        htmlifly(sl);
        htmlifly(sr);
        converttags(sl);
        converttags(sr);

        string html;
        readfile("x:/Vs/Differ/template_split.html", html);
        replaceone(html, "__LINE_R__", lr); // in reverse order for speed
        replaceone(html, "__LINE_L__", ll);
        replaceone(html, "__CODE_R__", sr);
        replaceone(html, "__CODE_L__", sl);
        return html;
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

    for (char c : s) // todo: move checks outside for better performance, or use multiple functions
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

struct linedata
{
    //int li; // line index
    string value;
    unsigned int hash;
    int cli; // corresponding line index
    linedata(/*int _li, */const string& _value, uint _hash, int _cli)
        : /*li(_li),*/ value(_value), hash(_hash), cli(_cli)
    { }
};
void readlinedata(vector<linedata>& ans, const char* filepath) // TODO: this misses the last line?
{
    ans.clear();
    std::ifstream file(filepath);
    std::string line;
    //int ln = 0;
    while (std::getline(file, line))
    {
        // TODO: both at the same time
        //unsigned int hash = gethash(line);
        ans.emplace_back(/*ln,*/ line, /*hash*/ 0u, -1);
        //++ln;
        //if (file.eof()) break;
    }
    if (file.eof() && file.fail())
    {
        ans.emplace_back("", 0u, -1); // last empty line
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

struct corrdata // correspondencedata
{
    int countA, countB;
    int liA, liB; // first occurrence only
    corrdata() : countA(0), countB(0), liA(-1), liB(-1) {}
};

vector<int> LIS(const vector<int>& A) // (we don't have duplicates)
{
	if (A.size() < 2) return A;

	vector<int> S; // stack top indices
	vector<int> P(A.size()); // pointers
	for (int i = 0; i < (int)A.size(); ++i)
	{
		// find leftmost stack index whose uppermost value is greater than the value
		// TODO: binsearch
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

struct slice
{
    int ia, ja; // [ia..ja[
    int ib, jb; // [ib..jb[
    slice(int _ia, int _ja, int _ib, int _jb) : ia(_ia), ja(_ja), ib(_ib), jb(_jb) { }
};

void findallcorrespondences(vector<linedata>& A, vector<linedata>& B)
{
    bool atleastonecorr = false;
    auto addcorr = [&](int liA, int liB)
    {
        A[liA].cli = liB;
        B[liB].cli = liA;
        //cout << "ln:" << liA + 1 << "-" << liB + 1 << endl;
        atleastonecorr = true;
    };

    queue<slice> Q;
    Q.push({ 0, (int)A.size(), 0, (int)B.size() });

    while (!Q.empty())
    {
        slice s = Q.front(); Q.pop();
        int ia = s.ia; int ja = s.ja;
        int ib = s.ib; int jb = s.jb;

        //cout << "[" << ia << "," << ja - 1 << "] [" << ib << "," << jb - 1 << "]\n";

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

        // TODO: only use vC
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
            // TODO: additional checks for not going out of bounds
            int sa = ia -1; // TODO: bounds
            while (sa < ja)
            {
                // find last matched (before gap)
                while (A[sa+1].cli != -1) ++sa;

                // find first matched (after gap)
                int ea = sa + 2;
                while (A[ea].cli == -1) ++ea;

                // find the intervals
                if (A[sa].cli+1 < A[ea].cli) // non-empty interval
                {
                    int x0 = sa + 1, x1 = ea, x2 = A[sa].cli + 1, x3 = A[ea].cli;
                    Q.push({ x0, x1, x2, x3 });
                    //cout << "  +[" << x0 << "," << x1-1 << "] [" << x2 << "," << x3-1 << "]\n";
                }
                sa = ea;
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
    
    //stringstream ssa, ssb, lsa, lsb;

    int ia = 0, ib = 0;
    while (ia < A.size() && ib < B.size())
    {
        if (A[ia].cli == ib /*&& B[ib].cli == ia*/) // correspondence
        {
            D.push_back(diffline::same(ia, ib, A[ia].value));
            //ssa << A[ia].value << '\n';
            //ssb << B[ib].value << '\n';
            //lsa << ia+1 << '\n';
            //lsb << ib+1 << '\n';
            ++ia; ++ib;
        }
        else // some diff
        {
            // find boundaries

            if (B[ib].cli != -1) // one-sided REM
            {
                int sa = ia, ea = B[ib].cli;

                //ssa << CHAR_REMB;
                for (; ia < ea; ++ia)
                {
                    D.push_back(diffline::rem(ia, A[ia].value));
                    //ssa << A[ia].value << '\n';
                    //lsa << ia+1 << '\n';
                }
                //ssa << CHAR_ENDB;
                //ssb << CHAR_NEUB << string(ea - sa, '\n') << CHAR_ENDB;
                //lsb << string(ea - sa, '\n');
            }
            else if (A[ia].cli != -1) // one-sided ADD
            {
                int sb = ib, eb = A[ia].cli;

                //ssb << CHAR_ADDB;
                for (; ib < eb; ++ib)
                {
                    D.push_back(diffline::add(ib, B[ib].value));
                    //ssb << B[ib].value << '\n';
                    //lsb << ib+1 << '\n';
                }
                //ssb << CHAR_ENDB;
                //ssa << CHAR_NEUB << string(eb - sb, '\n') << CHAR_ENDB;
                //lsa << string(eb - sb, '\n');
            }
            else // MIX
            {
                // TODO: appropriate background
                stringstream diffa, diffb;
                int sa = ia, sb = ib;
                while (ia < A.size() && A[ia].cli == -1)
                {
                    diffa << A[ia].value << '\n';
                    ++ia;
                }
                while (ib < B.size() && B[ib].cli == -1)
                {
                    diffb << B[ib].value << '\n';
                    ++ib;
                }
                int ea = ia, eb = ib;

                // run LCS
                string diffl = LCS(diffa.str(), diffb.str()); // the lcs contains \n for both lines
                string diffr = diffl;

                removeall(diffl, CHAR_ADD, CHAR_END);
                removeall(diffr, CHAR_REM, CHAR_END);

                // naive first implementation: split and put all at the beginning
                int da = ea - sa, db = eb - sb; // delta (number of lines)
                vector<string> va, vb;
                splitstring(diffl, va, '\n');
                splitstring(diffr, vb, '\n');
                int i = 0;
                for (; i < min(da, db); i++)
                {
                    int numRem = -1, numAdd = -1; // TODO
                    D.push_back(diffline::diff(sa+i, sb+i, "", va[i], vb[i], numRem, numAdd));
                }
                while (i < da) // more rem
                {
                    D.push_back(diffline::diff(sa + i, -1, "", va[i], "", 1, 0));
                    ++i;
                }
                while (i < db)
                {
                    D.push_back(diffline::diff(-1, sb+i, "", "", vb[i], 0, 1));
                    ++i;
                }


                /*
                ssa << CHAR_REMB << diffl << CHAR_ENDB;
                ssb << CHAR_ADDB << diffr << CHAR_ENDB;
                int da = ea - sa, db = eb - sb; // delta (number of lines)
                for (int i = sa; i < ea; ++i) lsa << i + 1 << '\n';
                for (int i = sb; i < eb; ++i) lsb << i + 1 << '\n';
                int d = db-da;
                if (d > 0)
                {
                    ssa << CHAR_NEUB << string(d, '\n') << CHAR_ENDB;
                    lsa << string(d, '\n');
                }
                else if (d < 0)
                {
                    ssb << CHAR_NEUB << string(-d, '\n') << CHAR_ENDB;
                    lsb << string(-d, '\n');
                }
                */

                /*
                // adds faux modifiers to notify it's a fake line
                removeallexcept(diffl, CHAR_ADD, CHAR_END, '\n')
                removeallexcept(diffr, CHAR_REM, CHAR_END, '\n');

                int da = ea - sa, db = eb - sb; // delta (number of lines)
                if (da == db) // same number of lines
                {
                    ssa << CHAR_REMB << diffl << CHAR_ENDB;
                    ssb << CHAR_ADDB << diffr << CHAR_ENDB;
                    for (int i = sa; i < ea; ++i) lsa << i+1 << '\n';
                    for (int i = sb; i < eb; ++i) lsb << i+1 << '\n';
                }
                else if (da > db) // full LEFT
                {
                    ssa << CHAR_REMB << diffl << CHAR_ENDB;
                    for (int i = sa; i < ea; ++i) lsa << i+1 << '\n';

                    // split
                    ssb << CHAR_ADDB << diffr << CHAR_ENDB;
                    for (int i = sb; i < eb; ++i) lsb << i+1 << '\n'; // add more
                    lsb << string(da - db, '\n');
                }
                else // (da < db) // full RIGHT
                {
                    ssb << CHAR_ADDB << diffr << CHAR_ENDB;
                    for (int i = sb; i < eb; ++i) lsb << i+1 << '\n';

                    ssa << CHAR_REMB << diffl << CHAR_ENDB;
                    for (int i = sa; i < ea; ++i) lsa << i+1 << '\n'; // add more
                    lsa << string(db-da, '\n');
                }
                */

                //ssa << CHAR_REMB << diffl << CHAR_ENDB;
                //ssb << CHAR_ADDB << diffr << CHAR_ENDB;
                //int nl = max(ea - sa, eb - sb);
                //lsa << string(nl, '\n');
                //lsb << string(nl, '\n');
            }
        }
    }
    // TODO: run final segment
    if (ia < A.size())
    {

    }
    else if (ib < B.size())
    {

    }

    // compute distance in double pass
    int dist = INT_MAX;

    auto assigndist = [&D, &dist](int i)
    {
        if (D[i].t != diffline::type::same) dist = 0;
        else if (dist < INT_MAX) ++dist;
        D[i].dist = min(D[i].dist, dist);
    };

    dist = INT_MAX;
    for (int i = 0; i < D.size();  ++i) assigndist(i);
    dist = INT_MAX;
    for (int i = D.size() - 1; i >= 0; --i) assigndist(i);

    //stra = ssa.str();
    //strb = ssb.str();
    //strla = lsa.str();
    //strlb = lsb.str();
}

auto starttime = chrono::high_resolution_clock::now();

void TIMER_START()
{
    starttime = chrono::high_resolution_clock::now();
}

void TIMER_END(const string& msg)
{
    auto endtime = chrono::high_resolution_clock::now();
    chrono::duration<double, milli>float_ms = endtime - starttime;
    cout << msg << " in " << float_ms.count() << " ms" << endl;
    starttime = endtime;
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
    //for (int i = 0; i < argc; ++i) cout << "argv[" << i << "]=" << argv[i] << "\n";
    
    if (filepath1 == nullptr || filepath2 == nullptr) // TODO: Check the files exist
    {
        return 1;
    }

    TIMER_START();

    string resulthtml;

    // save to file
    {
        string file1, file2;
        readfile(filepath1, file1);
        readfile(filepath2, file2);
        writefile("x:/Vs/Differ/output/lastfile_1.txt", file1);
        writefile("x:/Vs/Differ/output/lastfile_2.txt", file2);
    }

    if (diffmethod == DIFFMETHOD_LCS)
    {
        string file1, file2;
        readfile(filepath1, file1);
        readfile(filepath2, file2);
        TIMER_END("read");

        string diffresult = LCS(file1, file2);
        TIMER_END("diff");

        string resulthtml = convert2html_lcs(diffresult);
        TIMER_END("html");
    }
    else if (diffmethod == DIFFMETHOD_PATIENCE)
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
        patience(D, A, B);
        TIMER_END("pati");

        resulthtml = convert2html_patience(D);
        TIMER_END("html");
    }

    const char* outputfile = "x:/Vs/Differ/diffres.html";
    writefile(outputfile, resulthtml);
    TIMER_END("writ");

    ShellExecuteA(NULL, "open", outputfile, NULL, NULL, SW_SHOWNORMAL);
    TIMER_END("open");

    //system("PAUSE");
	return 0;
}