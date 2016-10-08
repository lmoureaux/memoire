#include "parser.h"

#include <cstdlib>
#include <string>

#include <TFile.h>
#include <TTree.h>

starlight_parser::starlight_parser(const std::string &filename) :
  _filename(filename),
  _in(filename)
{}

bool starlight_parser::end()
{
  char peek = _in.peek();
  while (peek == ' ' || peek == '\n') {
    _in.get();
    peek = _in.peek();
  }
  return _in.eof();
}

namespace {
  double read_double(std::istream &in)
  {
    std::string token;
    in >> token;
    return std::strtod(&token[0], 0);
  }
}

event starlight_parser::next()
{
  event evt;

  // Intermediate data
  std::string prefix;
  int ntracks;

  // Unused data
  int itrash;

  _in >> prefix;
  if (prefix != "EVENT:") {
    throw 1;
  }
  _in >> evt.id;
  _in >> ntracks;
  _in >> itrash; // vertex count

  _in >> prefix;
  if (prefix != "VERTEX:") {
    throw 2;
  }
  read_double(_in); // x
  read_double(_in); // y
  read_double(_in); // z
  read_double(_in); // t
  _in >> itrash /* vertex number */ >> itrash /* physical process */
       >> itrash /* parent track */ >> itrash /* daughter track count */;

  for (int i = 0; i < ntracks; i++) {
    _in >> prefix;
    if (prefix != "TRACK:") {
      throw 3;
    }
    track trk;
    _in >> trk.gpid;
    double px = read_double(_in);
    double py = read_double(_in);
    double pz = read_double(_in);
    trk.p = lorentz::vec::mxyz(.14, px, py, pz);
    _in >> itrash /* event number */ >> itrash /* starting vertex */
         >> itrash /* ending vertex */;
    _in >> trk.pdgid;

    // PYTHIA fields; read until eol
    char line[1024];
    _in.getline(line, sizeof(line));

    evt.tracks.push_back(trk);
  }

  return evt;
}

void starlight_parser::reset()
{
  if (_in.is_open()) {
    _in.close();
  }
  _in.open(_filename);
}

struct root_parser::data
{
  TFile *file;
  TTree *tree;
  long count, i;

  double rec_pxp;
  double rec_pyp;
  double rec_pzp;
  double rec_pxm;
  double rec_pym;
  double rec_pzm;
};

root_parser::root_parser(const std::string &filename) :
  _filename(filename),
  _d(new data)
{
  // Trees: rho_gen, rho_rec
  _d->file = new TFile(filename.c_str());
  _d->file->GetObject("rho_rec", _d->tree);
  _d->count = _d->tree->GetEntries();
  _d->i = 0;

  _d->tree->SetBranchAddress("rec_pxp", &_d->rec_pxp);
  _d->tree->SetBranchAddress("rec_pyp", &_d->rec_pyp);
  _d->tree->SetBranchAddress("rec_pzp", &_d->rec_pzp);

  _d->tree->SetBranchAddress("rec_pxm", &_d->rec_pxm);
  _d->tree->SetBranchAddress("rec_pym", &_d->rec_pym);
  _d->tree->SetBranchAddress("rec_pzm", &_d->rec_pzm);
}

bool root_parser::end()
{
  return _d->i >= _d->count;
}

event root_parser::next()
{
  _d->tree->GetEntry(_d->i++);

  event evt;

  track trk;
  trk.p = lorentz::vec::mxyz(.14, _d->rec_pxp, _d->rec_pyp, _d->rec_pzp);
  evt.tracks.push_back(trk);

  trk.p = lorentz::vec::mxyz(.14, _d->rec_pxm, _d->rec_pym, _d->rec_pzm);
  evt.tracks.push_back(trk);

  return evt;
}

void root_parser::reset()
{
  _d->i = 0;
}
