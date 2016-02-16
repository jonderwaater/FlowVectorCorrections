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

// \file QnCorrectionsDetector.cxx
// \brief Detector configuration classes implementation

#include "QnCorrectionsDetector.h"
#include "QnCorrectionsLog.h"

#define INITIALDATAVECTORBANKSIZE 100000
/// \cond CLASSIMP
ClassImp(QnCorrectionsDetector);
/// \endcond


/// Default constructor
QnCorrectionsDetector::QnCorrectionsDetector() : TNamed() {
  fDetectorId = -1;
}

/// Normal constructor
/// \param name the name of the detector
/// \param id detector Id
QnCorrectionsDetector::QnCorrectionsDetector(const char *name, Int_t id) :
    TNamed(name,name) {
  fDetectorId = id;
}

/// Default destructor
/// The detector class does not own anything
QnCorrectionsDetector::~QnCorrectionsDetector() {

}

/// Asks for support histograms creation
///
/// The request is transmitted to the attached detector configurations
/// \param list list where the histograms should be incorporated for its persistence
/// \return kTRUE if everything went OK
Bool_t QnCorrectionsDetector::CreateSupportHistograms(TList *list) {
  /* TODO: do we need to fine tune the list passed according to the detector? */
  Bool_t retValue = kFALSE;
  for (Int_t ixConfiguration = 0; ixConfiguration < fConfigurations.GetEntriesFast(); ixConfiguration++) {
    retValue = retValue || (fConfigurations.At(ixConfiguration)->CreateSupportHistograms(list));
  }
  return retValue;
}

/// Asks for attaching the needed input information to the correction steps
///
/// The request is transmitted to the attached detector configurations
/// \param list list where the input information should be found
/// \return kTRUE if everything went OK
Bool_t QnCorrectionsDetector::AttachCorrectionInputs(TList *list) {
  /* TODO: do we need to fine tune the list passed according to the detector? */
  Bool_t retValue = kFALSE;
  for (Int_t ixConfiguration = 0; ixConfiguration < fConfigurations.GetEntriesFast(); ixConfiguration++) {
    retValue = retValue || (fConfigurations.At(ixConfiguration)->AttachCorrectionInputs(list));
  }
  return retValue;
}

/// Adds a new detector configuration to the current detector
///
/// Raise an execution error if the configuration detector reference
/// does not match the current detector and if the detector configuration
/// is already incorporated to the detector.
/// \param detectorConfiguration pointer to the configuration to be added
void QnCorrectionsDetector::AddDetectorConfiguration(QnCorrectionsDetectorConfigurationBase *detectorConfiguration) {
  if (this != detectorConfiguration->GetDetector()) {
    QnCorrectionsFatal(Form("You are adding %s detector configuration of detector Id %d to detector Id %d. FIX IT, PLEASE.",
        detectorConfiguration->GetName(),
        detectorConfiguration->GetDetector()->GetId(),
        GetId()));
    return;
  }
  if (fConfigurations.FindObject(detectorConfiguration->GetName())) {
    QnCorrectionsFatal(Form("You are trying to add twice %s detector configuration to detector Id %d. FIX IT, PLEASE.",
        detectorConfiguration->GetName(),
        GetId()));
    return;
  }
  fConfigurations.Add(detectorConfiguration);
}

/// \cond CLASSIMP
ClassImp(QnCorrectionsDetectorConfigurationBase);
/// \endcond

/// Default constructor
QnCorrectionsDetectorConfigurationBase::QnCorrectionsDetectorConfigurationBase() : TNamed(),
    fQnVector(), fQnVectorCorrections() {
  fDetector = NULL;
  fCuts = NULL;
  fDataVectorBank = NULL;
  fEventClassVariables = NULL;
}

/// Normal constructor
/// \param name the name of the detector configuration
/// \param detector the detector object that will own the detector configuration
/// \param eventClassesVariables the set of event classes variables
/// \param nNoOfHarmonics the number of harmonics that must be handled
/// \param harmonicMap an optional ordered array with the harmonic numbers
QnCorrectionsDetectorConfigurationBase::QnCorrectionsDetectorConfigurationBase(const char *name,
      QnCorrectionsDetector *detector,
      QnCorrectionsEventClassVariablesSet *eventClassesVariables,
      Int_t nNoOfHarmonics,
      Int_t *harmonicMap) :
          TNamed(name,name),
          fQnVector(nNoOfHarmonics, harmonicMap), fQnVectorCorrections() {

  fDetector = detector;
  fCuts = NULL;
  fDataVectorBank = NULL;
  fEventClassVariables = eventClassesVariables;
}

/// Default destructor
/// Releases the memory which was taken
QnCorrectionsDetectorConfigurationBase::~QnCorrectionsDetectorConfigurationBase() {
  if (fDataVectorBank != NULL) {
    delete fDataVectorBank;
  }
}

/// Asks for support histograms creation
///
/// The request is transmitted, as per a base class, to the Q vector corrections.
/// \param list list where the histograms should be incorporated for its persistence
/// \return kTRUE if everything went OK
Bool_t QnCorrectionsDetectorConfigurationBase::CreateSupportHistograms(TList *list) {
  /* TODO: do we need to fine tune the list passed according to the detector? */
  Bool_t retValue = kFALSE;
  for (Int_t ixCorrection = 0; ixCorrection < fQnVectorCorrections.GetEntries(); ixCorrection++) {
    retValue = retValue || (fQnVectorCorrections.At(ixCorrection)->CreateSupportHistograms(list));
  }
  return retValue;
}

/// Asks for attaching the needed input information to the correction steps
///
/// The request is transmitted, as per a base class, to the Q vector corrections.
/// \param list list where the input information should be found
/// \return kTRUE if everything went OK
Bool_t QnCorrectionsDetectorConfigurationBase::AttachCorrectionInputs(TList *list) {
  /* TODO: do we need to fine tune the list passed according to the detector? */
  Bool_t retValue = kFALSE;
  for (Int_t ixCorrection = 0; ixCorrection < fQnVectorCorrections.GetEntries(); ixCorrection++) {
    retValue = retValue || (fQnVectorCorrections.At(ixCorrection)->AttachInput(list));
  }
  return retValue;
}

/// Incorporates the passed correction to the set of Q vector corrections
/// \param correctionOnQn the correction to add
void QnCorrectionsDetectorConfigurationBase::AddCorrectionOnQnVector(QnCorrectionsCorrectionOnQvector *correctionOnQn) {
  fQnVectorCorrections.AddCorrection(correctionOnQn);
}

/// Incorporates the passed correction to the set of input data corrections
///
/// Interface declaration function.
/// Default behavior. Base class should not be instantiated.
/// Run time error to support debugging.
///
/// \param correctionOnInputData the correction to add
void QnCorrectionsDetectorConfigurationBase::AddCorrectionOnInputData(QnCorrectionsCorrectionOnInputData *correctionOnInputData) {
  QnCorrectionsFatal(Form("You have reached base member %s. This means you have instantiated a base class or\n" \
      "you are using a non channelized detector configuration to calibrate input data. FIX IT, PLEASE.",
      "QnCorrectionsDetectorConfigurationBase::AddCorrectionOnInputData()"));
}

/// Checks if the current content of the variable bank applies to
/// the detector configuration
///
/// Interface declaration function.
/// Default behavior. Base class should not be instantiated.
/// Run time error to support debugging.
///
/// \param variableContainer pointer to the variable content bank
/// \return kTRUE if the current content applies to the configuration
Bool_t QnCorrectionsDetectorConfigurationBase::IsSelected(const Float_t *variableContainer) {
  QnCorrectionsFatal(Form("You have reached base member %s. This means you have instantiated a base class or\n" \
      "you are using a channelized detector configuration without passing the channel number. FIX IT, PLEASE.",
      "QnCorrectionsDetectorConfigurationBase::IsSelected()"));
  return kFALSE;
}

/// Checks if the current content of the variable bank applies to
/// the detector configuration for the passed channel.
///
/// Interface declaration function.
/// Default behavior. Base class should not be instantiated.
/// Run time error to support debugging.
///
/// \param variableContainer pointer to the variable content bank
/// \param nChannel the interested external channel number
/// \return kTRUE if the current content applies to the configuration
Bool_t QnCorrectionsDetectorConfigurationBase::IsSelected(const Float_t *variableContainer, Int_t nChannel) {
  QnCorrectionsFatal(Form("You have reached base member %s. This means you have instantiated a base class or\n" \
      "you are using a non channelized detector configuration but passing a channel number. FIX IT, PLEASE.",
      "QnCorrectionsDetectorConfigurationBase::IsSelected()"));
  return kFALSE;
}

/// \cond CLASSIMP
ClassImp(QnCorrectionsTrackDetectorConfiguration);
/// \endcond

/// Default constructor
QnCorrectionsTrackDetectorConfiguration::QnCorrectionsTrackDetectorConfiguration() : QnCorrectionsDetectorConfigurationBase() {

}

/// Normal constructor
/// Allocates the data vector bank.
/// \param name the name of the detector configuration
/// \param detector the detector object that will own the detector configuration
/// \param eventClassesVariables the set of event classes variables
/// \param nNoOfHarmonics the number of harmonics that must be handled
/// \param harmonicMap an optional ordered array with the harmonic numbers
QnCorrectionsTrackDetectorConfiguration::QnCorrectionsTrackDetectorConfiguration(const char *name,
      QnCorrectionsDetector *detector,
      QnCorrectionsEventClassVariablesSet *eventClassesVariables,
      Int_t nNoOfHarmonics,
      Int_t *harmonicMap) :
          QnCorrectionsDetectorConfigurationBase(name, detector, eventClassesVariables, nNoOfHarmonics, harmonicMap) {

  fDataVectorBank = new TClonesArray("QnCorrectionsDataVector", INITIALDATAVECTORBANKSIZE);
}

/// Default destructor
/// Memory taken is released by the parent class destructor
QnCorrectionsTrackDetectorConfiguration::~QnCorrectionsTrackDetectorConfiguration() {

}

/// \cond CLASSIMP
ClassImp(QnCorrectionsChannelDetectorConfiguration);
/// \endcond

/// Default constructor
QnCorrectionsChannelDetectorConfiguration::QnCorrectionsChannelDetectorConfiguration() :
    QnCorrectionsDetectorConfigurationBase(), fInputDataCorrections() {

  fUsedChannel = NULL;
  fChannelGroup = NULL;
  fNoOfChannels = 0;
}

/// Normal constructor
/// Allocates the data vector bank.
/// \param name the name of the detector configuration
/// \param detector the detector object that will own the detector configuration
/// \param eventClassesVariables the set of event classes variables
/// \param nNoOfHarmonics the number of harmonics that must be handled
/// \param harmonicMap an optional ordered array with the harmonic numbers
QnCorrectionsChannelDetectorConfiguration::QnCorrectionsChannelDetectorConfiguration(const char *name,
      QnCorrectionsDetector *detector,
      QnCorrectionsEventClassVariablesSet *eventClassesVariables,
      Int_t nNoOfChannels,
      Int_t nNoOfHarmonics,
      Int_t *harmonicMap) :
          QnCorrectionsDetectorConfigurationBase(name, detector, eventClassesVariables, nNoOfHarmonics, harmonicMap),
          fInputDataCorrections() {
  fUsedChannel = NULL;
  fChannelGroup = NULL;
  fNoOfChannels = nNoOfChannels;
  fDataVectorBank = new TClonesArray("QnCorrectionsChannelizedDataVector", INITIALDATAVECTORBANKSIZE);
}

/// Default destructor
/// Releases the memory taken
QnCorrectionsChannelDetectorConfiguration::~QnCorrectionsChannelDetectorConfiguration() {

  if (fUsedChannel != NULL) delete fUsedChannel;
  if (fChannelGroup != NULL) delete fChannelGroup;
}

/// Incorporates the channels scheme to the detector configuration
/// \param bUsedChannel array of booleans one per each channel
/// \param nChannelGroup array of group number for each channel
void QnCorrectionsChannelDetectorConfiguration::SetChannelsScheme(Bool_t *bUsedChannel, Int_t *nChannelGroup) {
  /* TODO: there should be smart procedures on how to improve the channels scan for actual data */
  fUsedChannel = new Bool_t[fNoOfChannels];
  fChannelGroup = new Int_t[fNoOfChannels];

  for (Int_t ixChannel = 0; ixChannel < fNoOfChannels; ixChannel++) {
    fUsedChannel[ixChannel] = bUsedChannel[ixChannel];
    fChannelGroup[ixChannel] = nChannelGroup[ixChannel];
  }
}

/// Asks for support histograms creation
///
/// The request is transmitted, as per a base class, to the input data corrections
/// then to the base clase to propagate it to the Q vector corrections
/// \param list list where the histograms should be incorporated for its persistence
/// \return kTRUE if everything went OK
Bool_t QnCorrectionsChannelDetectorConfiguration::CreateSupportHistograms(TList *list) {
  /* TODO: do we need to fine tune the list passed according to the detector? */
  Bool_t retValue = kFALSE;
  for (Int_t ixCorrection = 0; ixCorrection < fInputDataCorrections.GetEntries(); ixCorrection++) {
    retValue = retValue || (fInputDataCorrections.At(ixCorrection)->CreateSupportHistograms(list));
  }

  /* if everything right propagate it to Q vector corrections via base class */
  if (retValue) {
    return QnCorrectionsDetectorConfigurationBase::CreateSupportHistograms(list);
  }
  return retValue;
}

/// Asks for attaching the needed input information to the correction steps
///
/// The request is transmitted, as per a base class, to the input data corrections
/// then to the base clase to propagate it to the Q vector corrections
/// \param list list where the input information should be found
/// \return kTRUE if everything went OK
Bool_t QnCorrectionsChannelDetectorConfiguration::AttachCorrectionInputs(TList *list) {
  /* TODO: do we need to fine tune the list passed according to the detector? */
  Bool_t retValue = kFALSE;
  for (Int_t ixCorrection = 0; ixCorrection < fInputDataCorrections.GetEntries(); ixCorrection++) {
    retValue = retValue || (fInputDataCorrections.At(ixCorrection)->AttachInput(list));
  }

  /* if everything right propagate it to Q vector corrections via base class */
  if (retValue) {
    QnCorrectionsDetectorConfigurationBase::AttachCorrectionInputs(list);
  }
  return retValue;
}

/// Incorporates the passed correction to the set of input data corrections
/// \param correctionOnInputData the correction to add
void QnCorrectionsChannelDetectorConfiguration::AddCorrectionOnInputData(QnCorrectionsCorrectionOnInputData *correctionOnInputData) {
  fInputDataCorrections.AddCorrection(correctionOnInputData);
}

/// \cond CLASSIMP
ClassImp(QnCorrectionsDetectorConfigurationSet);
/// \endcond

/// Default constructor
QnCorrectionsDetectorConfigurationSet::QnCorrectionsDetectorConfigurationSet() :
    TObjArray() {

}

/// Default destructor
QnCorrectionsDetectorConfigurationSet::~QnCorrectionsDetectorConfigurationSet() {

}
