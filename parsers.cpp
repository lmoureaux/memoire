#include "parsers.h"

#include <cstdlib>
#include <string>

#include <TFile.h>
#include <TTree.h>

#include "sol.hpp"

#define MASS 0.13957018 // pion
//#define MASS 0.493677 // kaon

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

void starlight_parser::read()
{
  // Intermediate data
  std::string prefix;
  int ntracks;

  // Unused data
  int itrash;

  _current = event();

  _in >> prefix;
  if (prefix != "EVENT:") {
    throw 1;
  }
  _in >> _current.id;
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
    trk.p = lorentz::vec::mxyz(MASS, px, py, pz);
    _in >> itrash /* event number */ >> itrash /* starting vertex */
         >> itrash /* ending vertex */;
    _in >> trk.pdgid;

    // PYTHIA fields; read until eol
    char line[1024];
    _in.getline(line, sizeof(line));

    _current.add_track(trk);
  }
}

const event &starlight_parser::gen()
{
  return _current;
}

const event &starlight_parser::rec()
{
  return _current;
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
  TTree *gen_tree, *rec_tree;
  long gen_count, current_gen;
  long rec_count, current_rec;
  event gen, rec;

  int gen_i;
  double gen_pxp;
  double gen_pyp;
  double gen_pzp;
  double gen_pxm;
  double gen_pym;
  double gen_pzm;

  int rec_i;
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
  _d->file->GetObject("rho_rec", _d->rec_tree);
  _d->file->GetObject("rho_gen", _d->gen_tree);
  _d->gen_count = _d->gen_tree->GetEntries();
  _d->rec_count = _d->rec_tree->GetEntries();
  reset();

  _d->gen_tree->SetBranchAddress("gen_i", &_d->gen_i);
  _d->gen_tree->SetBranchAddress("gen_pxp", &_d->gen_pxp);
  _d->gen_tree->SetBranchAddress("gen_pyp", &_d->gen_pyp);
  _d->gen_tree->SetBranchAddress("gen_pzp", &_d->gen_pzp);
  _d->gen_tree->SetBranchAddress("gen_pxm", &_d->gen_pxm);
  _d->gen_tree->SetBranchAddress("gen_pym", &_d->gen_pym);
  _d->gen_tree->SetBranchAddress("gen_pzm", &_d->gen_pzm);

  _d->rec_tree->SetBranchAddress("rec_i", &_d->rec_i);
  _d->rec_tree->SetBranchAddress("rec_pxp", &_d->rec_pxp);
  _d->rec_tree->SetBranchAddress("rec_pyp", &_d->rec_pyp);
  _d->rec_tree->SetBranchAddress("rec_pzp", &_d->rec_pzp);
  _d->rec_tree->SetBranchAddress("rec_pxm", &_d->rec_pxm);
  _d->rec_tree->SetBranchAddress("rec_pym", &_d->rec_pym);
  _d->rec_tree->SetBranchAddress("rec_pzm", &_d->rec_pzm);
}

bool root_parser::end()
{
  return _d->current_gen >= _d->gen_count;
}

void root_parser::read()
{
  _d->gen_tree->GetEntry(_d->current_gen++);
  _d->gen = event();

  track trk;
  trk.p = lorentz::vec::mxyz(MASS, _d->gen_pxp, _d->gen_pyp, _d->gen_pzp);
  _d->gen.add_track(trk);
  trk.p = lorentz::vec::mxyz(MASS, _d->gen_pxm, _d->gen_pym, _d->gen_pzm);
  _d->gen.add_track(trk);

  if (_d->rec_i < _d->gen_i) {
    _d->rec_tree->GetEntry(_d->current_rec++);
  }
  if (_d->rec_i == _d->gen_i) {
    _d->rec = event();

    trk.p = lorentz::vec::mxyz(MASS, _d->rec_pxp, _d->rec_pyp, _d->rec_pzp);
    trk.match(_d->gen.tracks);
    _d->rec.add_track(trk);
    trk.p = lorentz::vec::mxyz(MASS, _d->rec_pxm, _d->rec_pym, _d->rec_pzm);
    trk.match(_d->gen.tracks);
    _d->rec.add_track(trk);
  }
}

const event &root_parser::gen()
{
  return _d->gen;
}

bool root_parser::has_rec()
{
  return _d->rec_i == _d->gen_i;
}

const event &root_parser::rec()
{
  return _d->rec;
}

void root_parser::reset()
{
  _d->current_gen = 0;
  _d->gen_i = 0;
  _d->current_rec = 0;
  _d->rec_i = -1;
}

struct hlt_parser::data
{
  TFile *file;
  TTree *tracks_tree, *castor_tree, *hbhe_tree, *hf_tree, *eb_tree, *ee_tree;
  long count, current;
  event rec;

  const static int BIG_TRACK_COUNT = 1000;
  const static int BIG_HIT_COUNT = 224;

  int ntracks;
  double chi2[BIG_TRACK_COUNT];
  double lambda[BIG_TRACK_COUNT];
  int    ndof[BIG_TRACK_COUNT];
  double p[BIG_TRACK_COUNT];
  double trkx[BIG_TRACK_COUNT];
  double trky[BIG_TRACK_COUNT];
  double trkz[BIG_TRACK_COUNT];
  double phi[BIG_TRACK_COUNT];
  double qoverp[BIG_TRACK_COUNT];

  unsigned ncastorhits;
  int   castorhitmodule[BIG_HIT_COUNT];
  int   castorhitsector[BIG_HIT_COUNT];
  float castorhitdata[BIG_HIT_COUNT];
};

hlt_parser::hlt_parser(const std::string &filename) :
  _filename(filename),
  _d(new data)
{
  // Trees: rho_gen, rho_rec
  _d->file = new TFile(filename.c_str());
  _d->file->GetObject("generalTracksTree", _d->tracks_tree);
  _d->count = _d->tracks_tree->GetEntries();
  reset();

  _d->tracks_tree->SetBranchAddress("nTracks", &_d->ntracks);
  _d->tracks_tree->SetBranchAddress("chi2", &_d->chi2);
  _d->tracks_tree->SetBranchAddress("lambda", &_d->lambda);
  _d->tracks_tree->SetBranchAddress("ndof", &_d->ndof);
  _d->tracks_tree->SetBranchAddress("p", &_d->p);
  _d->tracks_tree->SetBranchAddress("x", &_d->trkx);
  _d->tracks_tree->SetBranchAddress("y", &_d->trky);
  _d->tracks_tree->SetBranchAddress("z", &_d->trkz);
  _d->tracks_tree->SetBranchAddress("qoverp", &_d->qoverp);
  _d->tracks_tree->SetBranchAddress("phi", &_d->phi);

  _d->file->GetObject("CastorRecTree", _d->castor_tree);
  _d->castor_tree->SetBranchAddress("nCastorRecHits", &_d->ncastorhits);
  _d->castor_tree->SetBranchAddress("CastorRecHitModule", &_d->castorhitmodule);
  _d->castor_tree->SetBranchAddress("CastorRecHitSector", &_d->castorhitsector);
  _d->castor_tree->SetBranchAddress("CastorRecHitData", &_d->castorhitdata);

  _d->file->GetObject("HBHERecHitTree", _d->hbhe_tree);
  _d->hbhe_tree->SetBranchAddress("HEEnergyMaxPlus", &_d->rec.hcal.endcap.plus);
  _d->hbhe_tree->SetBranchAddress("HEEnergyMaxMinus", &_d->rec.hcal.endcap.minus);
  _d->hbhe_tree->SetBranchAddress("HBEnergyMaxPlus", &_d->rec.hcal.barrel.plus);
  _d->hbhe_tree->SetBranchAddress("HBEnergyMaxMinus", &_d->rec.hcal.barrel.minus);

  _d->file->GetObject("HFRecHitTree", _d->hf_tree);
  _d->hf_tree->SetBranchAddress("HFEnergyMaxPlus", &_d->rec.hcal.forward.plus);
  _d->hf_tree->SetBranchAddress("HFEnergyMaxMinus", &_d->rec.hcal.forward.minus);

  _d->file->GetObject("EERecHitTree", _d->ee_tree);
  _d->ee_tree->SetBranchAddress("EEEnergyMaxPlus", &_d->rec.ecal.endcap.plus);
  _d->ee_tree->SetBranchAddress("EEEnergyMaxMinus", &_d->rec.ecal.endcap.minus);

  _d->file->GetObject("EBRecHitTree", _d->eb_tree);
  _d->eb_tree->SetBranchAddress("EBEnergyMaxPlus", &_d->rec.ecal.barrel.plus);
  _d->eb_tree->SetBranchAddress("EBEnergyMaxMinus", &_d->rec.ecal.barrel.minus);
}

bool hlt_parser::end()
{
  return _d->current >= _d->count;
}

void hlt_parser::read()
{
  _d->rec = event();
  _d->tracks_tree->GetEntry(_d->current);
  _d->castor_tree->GetEntry(_d->current);
  _d->hbhe_tree->GetEntry(_d->current);
  _d->hf_tree->GetEntry(_d->current);
  _d->eb_tree->GetEntry(_d->current);
  _d->ee_tree->GetEntry(_d->current);

  for (int i = 0; i < _d->ntracks; ++i) {
    track trk;
    trk.p = lorentz::vec::m_r_phi_theta(MASS, _d->p[i],
                                        _d->phi[i], _d->lambda[i]);
    trk.charge = _d->qoverp[i] > 0 ? 1 : -1;
    trk.chi2 = _d->chi2[i];
    trk.ndof = _d->ndof[i];
    trk.x = lorentz::vec::txyz(0, _d->trkx[i], _d->trky[i], _d->trkz[i]);
    _d->rec.add_track(trk);
  }
  // Castor
  for (unsigned i = 0; i < _d->ncastorhits; ++i) {
    _d->rec.castor_status.add_hit(_d->castorhitmodule[i],
                                  _d->castorhitsector[i],
                                  _d->castorhitdata[i]);
  }

  _d->current++;
}

void hlt_parser::prepare(sol::state &lua)
{
  lua.script("require(\"lorentz\")");
}

void hlt_parser::fill_rec(sol::state &lua)
{
  lua["castor_energy"] = _d->rec.castor_status.energy();

  lua["ecal"] = lua.create_table();
  lua["ecal"]["bp"] = _d->rec.ecal.barrel.plus;
  lua["ecal"]["bm"] = _d->rec.ecal.barrel.minus;
  lua["ecal"]["ep"] = _d->rec.ecal.endcap.plus;
  lua["ecal"]["em"] = _d->rec.ecal.endcap.minus;

  lua["hcal"] = lua.create_table();
  lua["hcal"]["bp"] = _d->rec.hcal.barrel.plus;
  lua["hcal"]["bm"] = _d->rec.hcal.barrel.minus;
  lua["hcal"]["ep"] = _d->rec.hcal.endcap.plus;
  lua["hcal"]["em"] = _d->rec.hcal.endcap.minus;
  lua["hcal"]["fp"] = _d->rec.hcal.forward.plus;
  lua["hcal"]["fm"] = _d->rec.hcal.forward.minus;

  lua["tracks"] = lua.create_table();
  for (int i = 0; i < _d->ntracks; ++i) {
    lorentz::vec p = lorentz::vec::m_r_phi_theta(MASS, _d->p[i],
                                                 _d->phi[i], _d->lambda[i]);
    lua["tracks"][i] = lua.create_table();
    sol::table lua_p = lua["vec"]["new"](p.t(), p.x(), p.y(), p.z());
    lua["tracks"][i]["p"] = lua_p;
    lua["tracks"][i]["q"] = _d->qoverp[i] > 0 ? 1 : -1;
    lua["tracks"][i]["chi2"] = _d->chi2[i];
    lua["tracks"][i]["ndof"] = _d->ndof[i];
    lua["tracks"][i]["x"] = _d->trkx[i];
    lua["tracks"][i]["y"] = _d->trky[i];
    lua["tracks"][i]["z"] = _d->trkz[i];
  }
}

const event &hlt_parser::gen()
{
  return _d->rec;
}

const event &hlt_parser::rec()
{
  return _d->rec;
}

void hlt_parser::reset()
{
  _d->current = 0;
}
