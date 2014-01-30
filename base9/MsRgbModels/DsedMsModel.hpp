#ifndef GDSEDMAG_H
#define GDSEDMAG_H

#include <string>
#include <vector>

#include "../MsRgbModel.hpp"

const int N_DSED_Z         = 9;    /* number of metallicities in Chaboyer-Dotter isochrones */
const int N_DSED_AGES      = 52;   /* number of ages in Chaboyer-Dotter isochonres */
const int N_DSED_FILTS     = 8;
const int MAX_DSED_ENTRIES = 370;

class DsedMsModel : public MsRgbModel
{
  public:
    DsedMsModel() {;}
    virtual ~DsedMsModel() {;}

    virtual double deriveAgbTipMass(const std::vector<int>&, double, double, double);
    virtual double msRgbEvol(const std::vector<int>&, std::array<double, FILTS>&, double);
    virtual double wdPrecLogAge(double, double);
    virtual void loadModel(std::string, MsFilter);

  private:
    std::string getFileName (std::string, int, int, MsFilter);
};

#endif