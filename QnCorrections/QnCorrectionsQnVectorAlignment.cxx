/**************************************************************************************************
 *                                                                                                *
 * Package:       FlowVectorCorrections                                                           *
 * Authors:       Jaap Onderwaater, GSI, jacobus.onderwaater@cern.ch                              *
 *                Ilya Selyuzhenkov, GSI, ilya.selyuzhenkov@gmail.com                             *
 *                Víctor González, UCM, victor.gonzalez@cern.ch                                   *
 *                Contributors are mentioned in the code where appropriate.                       *
 * Development:   2012-2016                                                                       *
 *                                                                                                *
 * This file is part of FlowVectorCorrections, a software package that corrects Q-vector          *
 * measurements for effects of nonuniform detector acceptance. The corrections in this package    *
 * are based on publication:                                                                      *
 *                                                                                                *
 *  [1] "Effects of non-uniform acceptance in anisotropic flow measurements"                      *
 *  Ilya Selyuzhenkov and Sergei Voloshin                                                         *
 *  Phys. Rev. C 77, 034904 (2008)                                                                *
 *                                                                                                *
 * The procedure proposed in [1] is extended with the following steps:                            *
 * (*) alignment correction between subevents                                                     *
 * (*) possibility to extract the twist and rescaling corrections                                 *
 *      for the case of three detector subevents                                                  *
 *      (currently limited to the case of two “hit-only” and one “tracking” detectors)            *
 * (*) (optional) channel equalization                                                            *
 * (*) flow vector width equalization                                                             *
 *                                                                                                *
 * FlowVectorCorrections is distributed under the terms of the GNU General Public License (GPL)   *
 * (https://en.wikipedia.org/wiki/GNU_General_Public_License)                                     *
 * either version 3 of the License, or (at your option) any later version.                        *
 *                                                                                                *
 **************************************************************************************************/

/// \file QnCorrectionsQnVectorAlignment.cxx
/// \brief Implementation of procedures for Qn vector alignment correction.
#include "QnCorrectionsEventClasses.h"
#include "QnCorrectionsHistograms.h"
#include "QnCorrectionsCorrectionSteps.h"
#include "QnCorrectionsDetector.h"
#include "QnCorrectionsManager.h"
#include "QnCorrectionsLog.h"
#include "QnCorrectionsQnVectorAlignment.h"

const char *QnCorrectionsQnVectorAlignment::szCorrectionName = "Alignment";
const char *QnCorrectionsQnVectorAlignment::szKey = "EEEE";
const char *QnCorrectionsQnVectorAlignment::szSupportHistogramName = "QnQm Correlation Components";
const char *QnCorrectionsQnVectorAlignment::szCorrectedQnVectorName = "align";


/// \cond CLASSIMP
ClassImp(QnCorrectionsQnVectorAlignment);
/// \endcond

/// Default constructor
/// Passes to the base class the identity data for the recentering and width equalization correction step
QnCorrectionsQnVectorAlignment::QnCorrectionsQnVectorAlignment() :
    QnCorrectionsCorrectionOnQvector(szCorrectionName, szKey) {
  fInputHistograms = NULL;
  fCalibrationHistograms = NULL;
  fHarmonicForAlignment = -1;
  fDetectorConfigurationForAlignment = NULL;
}

/// Default destructor
/// Releases the memory taken
QnCorrectionsQnVectorAlignment::~QnCorrectionsQnVectorAlignment() {
  if (fInputHistograms != NULL)
    delete fInputHistograms;
  if (fCalibrationHistograms != NULL)
    delete fCalibrationHistograms;
}

/// Set the detector configuration used as reference for alignment
/// \param name the name of the reference detector configuration
void QnCorrectionsQnVectorAlignment::SetReferenceConfigurationForAlignment(const char *name) {

  if (QnCorrectionsManager::GetInstance()->FindDetectorConfiguration(name) != NULL) {
    fDetectorConfigurationForAlignment = QnCorrectionsManager::GetInstance()->FindDetectorConfiguration(name);
  }
  else {
    QnCorrectionsFatal(Form("Wrong reference detector configuration %s for %s alignment correction step", name, fDetectorConfiguration->GetName()));
  }
}


/// Asks for support data structures creation
///
/// Creates the recentered Qn vector
void QnCorrectionsQnVectorAlignment::CreateSupportDataStructures() {

  Int_t nNoOfHarmonics = fDetectorConfiguration->GetNoOfHarmonics();
  Int_t *harmonicsMap = new Int_t[nNoOfHarmonics];
  /* make sure the alignment harmonic processing is active */
  fDetectorConfiguration->ActivateHarmonic(fHarmonicForAlignment);
  /* in both configurations */
  fDetectorConfigurationForAlignment->ActivateHarmonic(fHarmonicForAlignment);
  /* and now create the corrected Qn vector */
  fDetectorConfiguration->GetHarmonicMap(harmonicsMap);
  fCorrectedQnVector = new QnCorrectionsQnVector(szCorrectedQnVectorName, nNoOfHarmonics, harmonicsMap);
  delete harmonicsMap;
}

/// Asks for support histograms creation
///
/// Allocates the histogram objects and creates the calibration histograms.
///
/// Process concurrency requires Calibration Histograms creation for all
/// concurrent processes but not for Input Histograms so, we delete previously
/// allocated ones.
/// \param list list where the histograms should be incorporated for its persistence
/// \return kTRUE if everything went OK
Bool_t QnCorrectionsQnVectorAlignment::CreateSupportHistograms(TList *list) {

  if (fInputHistograms != NULL) delete fInputHistograms;
  fInputHistograms = new QnCorrectionsProfileCorrelationComponents(szSupportHistogramName, szSupportHistogramName,
      fDetectorConfiguration->GetEventClassVariablesSet());
  fCalibrationHistograms = new QnCorrectionsProfileCorrelationComponents(szSupportHistogramName, szSupportHistogramName,
      fDetectorConfiguration->GetEventClassVariablesSet());

  /* get information about the configured harmonics to pass it for histogram creation */
  Int_t nNoOfHarmonics = fDetectorConfiguration->GetNoOfHarmonics();
  Int_t *harmonicsMap = new Int_t[nNoOfHarmonics];
  fDetectorConfiguration->GetHarmonicMap(harmonicsMap);
  fCalibrationHistograms->CreateCorrelationComponentsProfileHistograms(list,nNoOfHarmonics, harmonicsMap);
  delete harmonicsMap;
  return kTRUE;
}

/// Attaches the needed input information to the correction step
/// \param list list where the inputs should be found
/// \return kTRUE if everything went OK
Bool_t QnCorrectionsQnVectorAlignment::AttachInput(TList *list) {

  if (fInputHistograms->AttachHistograms(list)) {
    fState = QCORRSTEP_applyCollect;
    return kTRUE;
  }
  return kFALSE;
}

/// Asks for QA histograms creation
///
/// Allocates the histogram objects and creates the QA histograms.
/// \param list list where the histograms should be incorporated for its persistence
/// \return kTRUE if everything went OK
Bool_t QnCorrectionsQnVectorAlignment::CreateQAHistograms(TList *list) {
  return kTRUE;
}

/// Processes the correction step
///
/// Pure virtual function
/// \return kTRUE if the correction step was applied
Bool_t QnCorrectionsQnVectorAlignment::Process(const Float_t *variableContainer) {
  Int_t harmonic;
  switch (fState) {
  case QCORRSTEP_calibration:
    /* collect the data needed to further produce correction parameters if both current Qn vectors are good enough */
    if ((fDetectorConfiguration->GetCurrentQnVector()->IsGoodQuality()) &&
        (fDetectorConfigurationForAlignment->GetCurrentQnVector()->IsGoodQuality())) {
      harmonic = fDetectorConfiguration->GetCurrentQnVector()->GetFirstHarmonic();
      while (harmonic != -1) {
        fCalibrationHistograms->FillXX(harmonic,variableContainer,
            fDetectorConfiguration->GetCurrentQnVector()->Qx(harmonic)
            * fDetectorConfigurationForAlignment->GetCurrentQnVector()->Qx(fHarmonicForAlignment) );
        fCalibrationHistograms->FillXY(harmonic,variableContainer,
            fDetectorConfiguration->GetCurrentQnVector()->Qx(harmonic)
            * fDetectorConfigurationForAlignment->GetCurrentQnVector()->Qy(fHarmonicForAlignment));
        fCalibrationHistograms->FillYX(harmonic,variableContainer,
            fDetectorConfiguration->GetCurrentQnVector()->Qy(harmonic)
            * fDetectorConfigurationForAlignment->GetCurrentQnVector()->Qx(fHarmonicForAlignment) );
        fCalibrationHistograms->FillYY(harmonic,variableContainer,
            fDetectorConfiguration->GetCurrentQnVector()->Qy(harmonic)
            * fDetectorConfigurationForAlignment->GetCurrentQnVector()->Qy(fHarmonicForAlignment));
        harmonic = fDetectorConfiguration->GetCurrentQnVector()->GetNextHarmonic(harmonic);
      }
    }
    /* we have not perform any correction yet */
    return kFALSE;
    break;
  case QCORRSTEP_applyCollect:
    /* collect the data needed to further produce correction parameters if both current Qn vectors are good enough */
    if ((fDetectorConfiguration->GetCurrentQnVector()->IsGoodQuality()) &&
        (fDetectorConfigurationForAlignment->GetCurrentQnVector()->IsGoodQuality())) {
      harmonic = fDetectorConfiguration->GetCurrentQnVector()->GetFirstHarmonic();
      while (harmonic != -1) {
        fCalibrationHistograms->FillXX(harmonic,variableContainer,
            fDetectorConfiguration->GetCurrentQnVector()->Qx(harmonic)
            * fDetectorConfigurationForAlignment->GetCurrentQnVector()->Qx(fHarmonicForAlignment) );
        fCalibrationHistograms->FillXY(harmonic,variableContainer,
            fDetectorConfiguration->GetCurrentQnVector()->Qx(harmonic)
            * fDetectorConfigurationForAlignment->GetCurrentQnVector()->Qy(fHarmonicForAlignment));
        fCalibrationHistograms->FillYX(harmonic,variableContainer,
            fDetectorConfiguration->GetCurrentQnVector()->Qy(harmonic)
            * fDetectorConfigurationForAlignment->GetCurrentQnVector()->Qx(fHarmonicForAlignment) );
        fCalibrationHistograms->FillYY(harmonic,variableContainer,
            fDetectorConfiguration->GetCurrentQnVector()->Qy(harmonic)
            * fDetectorConfigurationForAlignment->GetCurrentQnVector()->Qy(fHarmonicForAlignment));
        harmonic = fDetectorConfiguration->GetCurrentQnVector()->GetNextHarmonic(harmonic);
      }
    }
    /* and proceed to ... */
  case QCORRSTEP_apply: /* apply the correction if the current Qn vector is good enough */
    if (fDetectorConfiguration->GetCurrentQnVector()->IsGoodQuality()) {
      /* we get the properties of the current Qn vector but its name */
      fCorrectedQnVector->Set(fDetectorConfiguration->GetCurrentQnVector(),kFALSE);
      Double_t XX  = fInputHistograms->GetXXBinContent(fHarmonicForAlignment, fInputHistograms->GetBin(variableContainer));
      Double_t YY  = fInputHistograms->GetYYBinContent(fHarmonicForAlignment, fInputHistograms->GetBin(variableContainer));
      Double_t XY  = fInputHistograms->GetXYBinContent(fHarmonicForAlignment, fInputHistograms->GetBin(variableContainer));
      Double_t YX  = fInputHistograms->GetYXBinContent(fHarmonicForAlignment, fInputHistograms->GetBin(variableContainer));
      Double_t eXX = fInputHistograms->GetXXBinError(fHarmonicForAlignment, fInputHistograms->GetBin(variableContainer));
      Double_t eYY = fInputHistograms->GetYYBinError(fHarmonicForAlignment, fInputHistograms->GetBin(variableContainer));
      Double_t eXY = fInputHistograms->GetXYBinError(fHarmonicForAlignment, fInputHistograms->GetBin(variableContainer));
      Double_t eYX = fInputHistograms->GetYXBinError(fHarmonicForAlignment, fInputHistograms->GetBin(variableContainer));

      Double_t deltaPhi = - TMath::ATan2((XY-YX),(XX+YY)) * (1.0 / fHarmonicForAlignment);

      /* protections!!! */
      if (!((XY-YX)*(XY-YX)/(eXY*eXY+eYX*eYX) < 2.0)) {
        harmonic = fDetectorConfiguration->GetCurrentQnVector()->GetFirstHarmonic();
        while (harmonic != -1) {
          fCorrectedQnVector->SetQx(harmonic,
              fDetectorConfiguration->GetCurrentQnVector()->Qx(harmonic) * TMath::Cos(((Double_t) harmonic) * deltaPhi)
              + fDetectorConfiguration->GetCurrentQnVector()->Qy(harmonic) * TMath::Sin (((Double_t) harmonic) * deltaPhi));
          fCorrectedQnVector->SetQy(harmonic,
              fDetectorConfiguration->GetCurrentQnVector()->Qy(harmonic) * TMath::Cos(((Double_t) harmonic) * deltaPhi)
              - fDetectorConfiguration->GetCurrentQnVector()->Qx(harmonic) * TMath::Sin (((Double_t) harmonic) * deltaPhi));
          harmonic = fDetectorConfiguration->GetCurrentQnVector()->GetNextHarmonic(harmonic);
        }
      }
    }
    else {
      /* not done! */
      fCorrectedQnVector->SetGood(kFALSE);
    }
    /* and update the current Qn vector */
    fDetectorConfiguration->UpdateCurrentQnVector(fCorrectedQnVector);
    break;
  }
  /* if we reached here is because we applied the correction */
  return kTRUE;
}

/// Clean the correction to accept a new event
void QnCorrectionsQnVectorAlignment::ClearCorrectionStep() {

  fCorrectedQnVector->Reset();
}
