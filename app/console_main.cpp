#include <fstream>
#include <iostream>

#include "table_file_io.h"

#include "../core/spreadsheet.h"

using namespace std;


int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    ifstream fin;
    ofstream fout;

    istream* in = &cin;
    ostream* out = &cout;

    if (argc >= 2) {
        fin.open(argv[1]);
        if (!fin) return 1;
        in = &fin;
    }
    if (argc >= 3) {
        fout.open(argv[2]);
        if (!fout) return 1;
        out = &fout;
    }

    emw::SpreadsheetGrid grid;
    int rows = 0;
    int cols = 0;
    if (!emw_app::LoadSizedTextGrid(*in, grid, rows, cols)) return 1;

    emw_app::WriteGridValuesPlain(*out, grid, rows, cols);
    return 0;
}
