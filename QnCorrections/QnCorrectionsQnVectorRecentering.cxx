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

/// \file QnCorrectionsQnVectorRecentering.cxx
/// \brief Implementation of procedures for Qn vector recentering.
#include "QnCorrectionsEventClasses.h"
#include "QnCorrectionsHistograms.h"
#include "QnCorrectionsCorrectionSteps.h"
#include "QnCorrectionsDetector.h"
#include "QnCorrectionsLog.h"
#include "QnCorrectionsQnVectorRecentering.h"

const char *QnCorrectionsQnVectorRecentering::szCorrectionName = "Recentering and width equalization";
const char *QnCorrectionsQnVectorRecentering::szKey = "CCCC";
const char *QnCorrectionsQnVectorRecentering::szSupportHistogramName = "Qn";
const char *QnCorrectionsQnVectorRecentering::szCorrectedQnVectorName = "rec";


/// \cond CLASSIMP
ClassImp(QnCorrectionsQnVectorRecentering);
/// \endcond

/// Default constructor
/// Passes to the base class the identity data for the recentering and width equalization correction step
QnCorrectionsQnVectorRecentering::QnCorrectionsQnVectorRecentering() :
    QnCorrectionsCorrectionOnQvector(szCorrectionName, szKey) {
  fInputHistograms = NULL;
  fCalibrationHistograms = NULL;
  fApplyWidthEqualization = kFALSE;
}

/// Default destructor
/// Releases the memory taken
QnCorrectionsQnVectorRecentering::~QnCorrectionsQnVectorRecentering() {
  if (fInputHistograms != NULL)
    delete fInputHistograms;
  if (fCalibrationHistograms != NULL)
    delete fCalibrationHistograms;
}

/// Asks for support data structures creation
///
/// Creates the recentered Qn vector
void QnCorrectionsQnVectorRecentering::CreateSupportDataStructures() {

  Int_t nNoOfHarmonics = fDetectorConfiguration->GetNoOfHarmonics();
  Int_t *harmonicsMap = new Int_t[nNoOfHarmonics];
  fDetectorConfiguration->GetHarmonicMap(harmonicsMap);
  fCorrectedQnVector = new QnCorrectionsQnVector(szCorrectedQnVectorName, nNoOfHarmonics, harmonicsMap);
  delete harmonicsMap;
}

/// Asks for support histograms creation
///
/// Allocates the histogram objects and creates the calibration histograms.
/// The histograms are constructed with standard deviation error calculation
/// for the proper behavior of optional gain equalization step.
///
/// Process concurrency requires Calibration Histograms creation for all c
/// concurrent processes but not for Input Histograms so, we delete previously
/// allocated ones.
/// \param list list where the histograms should be incorporated for its persistence
/// \return kTRUE if everything went OK
Bool_t QnCorrectionsQnVectorRecentering::CreateSupportHistograms(TList *list) {

  TString histoNameAndTitle = Form("%s %s ",
      szSupportHistogramName,
      fDetectorConfiguration->GetName());

  if (fInputHistograms != NULL) delete fInputHistograms;
  fInputHistograms = new QnCorrectionsProfileComponents((const char *) histoNameAndTitle, (const char *) histoNameAndTitle,
      fDetectorConfiguration->GetEventClassVariablesSet(), "s");
  fCalibrationHistograms = new QnCorrectionsProfileComponents((const char *) histoNameAndTitle, (const char *) histoNameAndTitle,
      fDetectorConfiguration->GetEventClassVariablesSet(), "s");

  /* get information about the configured harmonics to pass it for histogram creation */
  Int_t nNoOfHarmonics = fDetectorConfiguration->GetNoOfHarmonics();
  Int_t *harmonicsMap = new Int_t[nNoOfHarmonics];
  fDetectorConfiguration->GetHarmonicMap(harmonicsMap);
  fCalibrationHistograms->CreateComponentsProfileHistograms(list,nNoOfHarmonics, harmonicsMap);
  delete harmonicsMap;
  return kTRUE;
}

/// Attaches the needed input information to the correction step
/// \param list list where the inputs should be found
/// \return kTRUE if everything went OK
Bool_t QnCorrectionsQnVectorRecentering::AttachInput(TList *list) {

  if (fInputHistograms->AttachHistograms(list)) {
    QnCorrectionsInfo(Form("Recentering on %s going to be applied", fDetectorConfiguration->GetName()));
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
Bool_t QnCorrectionsQnVectorRecentering::CreateQAHistograms(TList *list) {
  return kTRUE;
}

/// Processes the correction step
///
/// Pure virtual function
/// \return kTRUE if the correction step was applied
Bool_t QnCorrectionsQnVectorRecentering::Process(const Float_t *variableContainer) {
  Int_t harmonic;
  switch (fState) {
  case QCORRSTEP_calibration:
    QnCorrectionsInfo(Form("Recentering process in detector %s: collecting data.", fDetectorConfiguration->GetName()));
    /* collect the data needed to further produce correction parameters if the current Qn vector is good enough */
    if (fDetectorConfiguration->GetCurrentQnVector()->IsGoodQuality()) {
      harmonic = fDetectorConfiguration->GetCurrentQnVector()->GetFirstHarmonic();
      while (harmonic != -1) {
        fCalibrationHistograms->FillX(harmonic,variableContainer,fDetectorConfiguration->GetCurrentQnVector()->Qx(harmonic));
        fCalibrationHistograms->FillY(harmonic,variableContainer,fDetectorConfiguration->GetCurrentQnVector()->Qy(harmonic));
        harmonic = fDetectorConfiguration->GetCurrentQnVector()->GetNextHarmonic(harmonic);
      }
    }
    /* we have not perform any correction yet */
    return kFALSE;
    break;
  case QCORRSTEP_applyCollect:
    QnCorrectionsInfo(Form("Recentering process in detector %s: collecting data.", fDetectorConfiguration->GetName()));
    /* collect the data needed to further produce correction parameters if the current Qn vector is good enough */
    if (fDetectorConfiguration->GetCurrentQnVector()->IsGoodQuality()) {
      harmonic = fDetectorConfiguration->GetCurrentQnVector()->GetFirstHarmonic();
      while (harmonic != -1) {
        fCalibrationHistograms->FillX(harmonic,variableContainer,fDetectorConfiguration->GetCurrentQnVector()->Qx(harmonic));
        fCalibrationHistograms->FillY(harmonic,variableContainer,fDetectorConfiguration->GetCurrentQnVector()->Qy(harmonic));
        harmonic = fDetectorConfiguration->GetCurrentQnVector()->GetNextHarmonic(harmonic);
      }
    }
    /* and proceed to ... */
  case QCORRSTEP_apply: /* apply the correction if the current Qn vector is good enough */
    QnCorrectionsInfo(Form("Recentering process in detector %s: applying correction.", fDetectorConfiguration->GetName()));
    if (fDetectorConfiguration->GetCurrentQnVector()->IsGoodQuality()) {
      /* we get the properties of the current Qn vector but its name */
      fCorrectedQnVector->Set(fDetectorConfiguration->GetCurrentQnVector(),kFALSE);
      harmonic = fDetectorConfiguration->GetCurrentQnVector()->GetFirstHarmonic();
      while (harmonic != -1) {
        Float_t widthX = 1.0;
        Float_t widthY = 1.0;
        if (fApplyWidthEqualization) {
          widthX = fInputHistograms->GetXBinError(harmonic, fInputHistograms->GetBin(variableContainer));
          widthY = fInputHistograms->GetYBinError(harmonic, fInputHistograms->GetBin(variableContainer));
        }
        fCorrectedQnVector->SetQx(harmonic, (fDetectorConfiguration->GetCurrentQnVector()->Qx(harmonic)
            - fInputHistograms->GetXBinContent(harmonic, fInputHistograms->GetBin(variableContainer)))
            / widthX);
        fCorrectedQnVector->SetQy(harmonic, (fDetectorConfiguration->GetCurrentQnVector()->Qy(harmonic)
            - fInputHistograms->GetYBinContent(harmonic, fInputHistograms->GetBin(variableContainer)))
            / widthY);
        harmonic = fDetectorConfiguration->GetCurrentQnVector()->GetNextHarmonic(harmonic);
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
void QnCorrectionsQnVectorRecentering::ClearCorrectionStep() {

  fCorrectedQnVector->Reset();
}

