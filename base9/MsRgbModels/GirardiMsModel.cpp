#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <vector>

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "Cluster.hpp"
#include "Isochrone.hpp"
#include "Star.hpp"

#include "LinearTransform.hpp"
#include "GirardiMsModel.hpp"

using std::array;
using std::string;
using std::vector;
using std::stringstream;
using std::lower_bound;
using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;

const unsigned int maxIgnore = std::numeric_limits<char>::max();

static vector<string> getFileNames (string path, FilterSetName filterSet)
{
    const array<string, 8> fileNames = { "0", "0001", "0004", "001", "004", "008", "019", "030" };
    vector<string> files;

    for (auto file : fileNames)
    {
        string tempFile = path;

        if (filterSet == FilterSetName::ACS)
            tempFile += "gIsoACS/iso_acs_z";
        else
            tempFile += "gIsoStan/iso_stan_z";

        tempFile += file;
        tempFile += ".50.dat";

        files.push_back(tempFile);
    }

    return files;
}


string GirardiMsModel::getFileName (string) const
{
    throw std::logic_error("Girardi models do not support single-file loading");
}


bool GirardiMsModel::isSupported(FilterSetName filterSet) const
{
    return (filterSet == FilterSetName::UBVRIJHK || filterSet == FilterSetName::ACS);
}


/****************************************************************************
 ***********************    filterSet = UBVRIJHK      ***********************
 *********************** Girardi models, standard mags **********************
 ** For simplicity, the 8 input files are assumed to have 50 ages each,    **
 ** which is the minimum set among all of these isochrones.  These ages    **
 ** span the range log(age) = 7.80 to 10.25.                               **
 ** The eight files and their metallicities are                            **
 **   iso_stan_z0.50.dat     Z = 0.0                                       **
 **   iso_stan_z0001.50.dat  Z = 0.0001                                    **
 **   iso_stan_z0004.50.dat  Z = 0.0004                                    **
 **   iso_stan_z001.50.dat   Z = 0.001                                     **
 **   iso_stan_z004.50.dat   Z = 0.004                                     **
 **   iso_stan_z008.50.dat   Z = 0.008                                     **
 **   iso_stan_z019.50.dat   Z = 0.019                                     **
 **   iso_stan_z030.50.dat   Z = 0.030                                     **
 ***************************************************************************/

/****************************************************************************
 **********************        filterSet == ACS       ***********************
 ********************** Girardi models, ACS mags ****************************
 ** For simplicity, the input files are assumed to have 50 ages, just as   **
 ** the above-used Girardi standard mag isochrones.  This meant reducing   **
 ** the 50-74 ages by limiting them to log(age) = 7.80 to 10.25, which is  **
 ** fine for now as the rejected isochrones are younger than any of the    **
 ** clusters for which we have data.  The metallicities are also all the   **
 ** same.  These are the same isochrones, just convolved by Girardi with   **
 ** different filters.  The file names are different only in that the      **
 ** "stan" becomes "acs".                                                  **
 ***************************************************************************/
void GirardiMsModel::loadModel (string path, FilterSetName filterSet)
{
    ifstream fin;

    assert(isSupported(filterSet));

    for (auto file : getFileNames(path, filterSet))
    {
        double fileZ, logAge, ignore;

        string line;
        stringstream lin;

        vector<Isochrone> isochrones;
        vector<EvolutionaryPoint> eeps;

        // Open the file
        fin.open(file);

        // Ensure the file opened successfully
        if (!fin)
        {
            cerr << "\n file " << file << " was not found - exiting" << endl;
            exit (1);
        }

        // Get Z from the first header line
        fin.ignore(maxIgnore, '='); // Eat the first bit of the line
        fin >> fileZ;
        fin.ignore(maxIgnore, '\n'); // Eat the last bit of the line
        getline(fin, line); // Eat the second header line to avoid the "new isochrone" code

        // Now, for every other line in the file...
        while (!fin.eof())
        {
            getline(fin, line);

            if (!line.empty() && (line.at(0) != '#'))
            {
                double tempMass;

                stringstream in(line);

                vector<double> mags;
                mags.resize(FILTS, 99.999);

                if (filterSet == FilterSetName::UBVRIJHK)
                {                       // Girardi UBVRIJHK isocrhones
                    in >> logAge
                       >> tempMass
                       >> ignore >> ignore >> ignore >> ignore >> ignore;

                    for (int filt = 0; filt < 8; ++filt)
                    {
                        in >> mags.at(filt);
                    }

                    // Ignore Flum
                }
                else if (filterSet == FilterSetName::ACS)
                {                       // Girardi hST/ACS/WF isochrones
                    in >> logAge
                       >> tempMass
                       >> ignore >> ignore >> ignore >> ignore >> ignore;

                    for (int filt = 0; filt < 6; ++filt)
                    {
                        in >> mags.at(filt);
                    }

                    in >> ignore >> ignore
                       >> mags.at(6)
                       >> mags.at(7);

                    // Ignore the remaining two filters and Flum
                }

                if (!fin.eof())
                {
                    // Girardi doesn't have EEPs, so we'll pretend with 0
                    eeps.emplace_back(0, tempMass, mags);
                }
            }
            else if (!line.empty() && (line.at(0) == '#')) // Time for a new isochrone
            {
                isochrones.emplace_back(logAge, eeps);
                eeps.clear();

                getline(fin, line); // Eat the extra header line
            }
        } // EOF

        if (fileZ == 0.0)
        {
            fileZ = -5.0; // Rescue the -inf case (which causes problems with interpolation)
        }
        else
        {
            fileZ = log10(fileZ / modelZSolar);
        }

        isochrones.emplace_back(logAge, eeps); // Push in the last age for this [Fe/H]
        vector<HeliumCurve> tVector;
        tVector.emplace_back(0.0, isochrones);
        fehCurves.emplace_back(fileZ, tVector); // Push the entire isochrone set into the model's FehCurve vector

        fin.close();
    }

    ageLimit.first = fehCurves.front().heliumCurves.front().isochrones.front().logAge;
    ageLimit.second = fehCurves.front().heliumCurves.front().isochrones.back().logAge;
}


Isochrone GirardiMsModel::deriveIsochrone(const vector<int>& filters, double newFeH, double, double newAge) const
{
    // Run code comparable to the implementation of deriveAgbTipMass for every mag in every eep, interpolating first in age and then in FeH
    // Check for requested age or [Fe/H] out of bounds
    if ((newAge < 7.80)
     || (newAge > 10.25)
     || (newFeH < fehCurves.front().feh)
     || (newFeH > fehCurves.back().feh))
    {
        throw InvalidCluster("Age or FeH out of bounds in GirardiMsModel::deriveIsochrone");
    }

    // Take the newAge and round it to the nearest 0.05
    double roundAge = (ceil (20. * newAge) / 20.) - 0.05;

    // In this line, PFM. Takes the interpAge which we previously rounded to the
    // next lowest 0.05, subtracts the minimum age of the Girardi isochrones,
    // multiplies by the integral of the Girardi logAge step size, and rounds to
    // an integer (which conveniently ends up being the age index).
    int iAge = (int) (rint ((roundAge - 7.8) * 20));

    auto fehIter = lower_bound(fehCurves.begin(), fehCurves.end(), newFeH, FehCurve::compareFeh);

    if (fehIter == fehCurves.end())
    {
        fehIter -= 2;
    }
    else if (fehIter != fehCurves.begin())
    {
        fehIter -= 1;
    }

    // Assure that the iAge is reasonable
    // Age gridding is identical between [Fe/H] grid points
    assert(iAge >= 0);
    assert(fehIter->heliumCurves.front().isochrones.size() > iAge);
    assert(fehIter->heliumCurves.front().isochrones.at(iAge).logAge     < newAge);
    assert(fehIter->heliumCurves.front().isochrones.at(iAge + 1).logAge >= newAge);

    vector<Isochrone> interpIso;

    // Interpolate between two ages in two FeHs.
    for (int i = 0; i < 2; ++i)
    {
        // Shortcut iterator to replace the full path from fehIter
        // There's only one Y value, so we're using .front()
        auto ageIter = fehIter[i].heliumCurves.front().isochrones.begin() + iAge;

        assert(ageIter[0].eeps.front().mass == ageIter[1].eeps.front().mass);

        // Girardi doesn't have real EEPs, so we have to assume the same number
        // of mass points and interpolate among them.
        size_t numEeps = std::min(ageIter[0].eeps.size(), ageIter[1].eeps.size());

        vector<EvolutionaryPoint> interpEeps;

        for (size_t e = 0; e < numEeps; ++e)
        {
            vector<double> mags;
            mags.resize(FILTS, 99.999);

            for (auto f : filters)
            {
                mags[f] = linearTransform<>(ageIter[0].logAge
                                          , ageIter[1].logAge
                                          , ageIter[0].eeps.at(e).mags.at(f)
                                          , ageIter[1].eeps.at(e).mags.at(f)
                                          , newAge).val;
            }

            // Still using 0 for EEP, since Girardi doesn't know about EEPs
            interpEeps.emplace_back(0, ageIter[0].eeps.at(e).mass, mags);
        }

        double interpAge = linearTransform<>(fehIter[0].feh
                                           , fehIter[1].feh
                                           , ageIter[0].logAge
                                           , ageIter[1].logAge
                                           , newFeH).val;

        // These won't necessarily be true if extrapolation is allowed (which it
        // is), but should be be true due to the age and FeH checks at the
        // beginning of this function
        assert(ageIter[0].logAge <= interpAge);
        assert(ageIter[1].logAge >= interpAge);

        interpIso.emplace_back(interpAge, interpEeps);
    }

    assert(interpIso.size() == 2);
    assert(interpIso[0].logAge == interpIso[1].logAge);

    // Now, interpolate between the two derived isochrones using FeH
    vector<EvolutionaryPoint> interpEeps;
    size_t numEeps = std::min(interpIso.at(0).eeps.size(), interpIso.at(1).eeps.size());

    for (size_t e = 0; e < numEeps; ++e)
    {
        vector<double> mags;
        mags.resize(FILTS, 99.999);

        for (auto f : filters)
        {
            mags[f] = linearTransform<>(fehIter[0].feh
                                      , fehIter[1].feh
                                      , interpIso.at(0).eeps.at(e).mags.at(f)
                                      , interpIso.at(1).eeps.at(e).mags.at(f)
                                      , newFeH).val;
        }

        double interpMass = linearTransform<>(fehIter[0].feh
                                            , fehIter[1].feh
                                            , interpIso.at(0).eeps.at(e).mass
                                            , interpIso.at(1).eeps.at(e).mass
                                            , newFeH).val;

        // Still using 0 for EEP, since Girardi doesn't know about EEPs
        interpEeps.emplace_back(0, interpMass, mags);
    }

    assert(std::is_sorted(interpEeps.begin(), interpEeps.end()));

    return {interpIso.at(0).logAge, interpEeps};
}
