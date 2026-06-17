/*
 * Copyright (c) 2020-2024 Key4hep-Project.
 *
 * This file is part of Key4hep.
 * See https://key4hep.github.io/key4hep-doc/ for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
//This is the algorithm/functional
// Gaudi
#include "Gaudi/Property.h"
#include <GaudiKernel/SmartIF.h>

// k4FWCore
#include "k4FWCore/Transformer.h"

// k4Interface
#include <k4Interface/IUniqueIDGenSvc.h>

// edm4hep
#include "edm4hep/CalorimeterHitCollection.h"
#include "edm4hep/SimCalorimeterHitCollection.h"

// dd4hep
#include <DD4hep/DD4hepUnits.h>

// STL
#include <random>
#include <string>

// struct RandomNoiseDigitizer final : defines a new class called RandomNoiseDigitizer. 
// The keyword final indicates that this class (RandomNoiseDigitizer) cannot be inherited from -> end of inheritance chain.
//Output : std::tuple<edm4hep::CalorimeterHitCollection>; 
// Inputs : const edm4hep::SimCalorimeterHitCollection&, const edm4hep::EventHeaderCollection&>: passed as refrences (readonly, no copying)
struct RandomNoiseDigitizer final
    : k4FWCore::MultiTransformer<std::tuple<edm4hep::CalorimeterHitCollection>(
          const edm4hep::SimCalorimeterHitCollection&, const edm4hep::EventHeaderCollection&)> {

public:
  // Constructor

  //The constructior is where the abstract I/O types in the temaplte definition above is connected to concrete named data paths that Gaudi will use at runtime
  // Every Gaudi constructor algo always takes in the same two arguments : name and service locator.
  // name : algo's instance name as string : let's you have mutiple instances of the same algo in a single job with different names and configurations
  // service locator : central registry through which an algo can find and sceess framework-wide services. 
  // : MultiTransformer() {}-> base class initialisation  list. 
  // Body of the consrtuctor is empty because all the necessary initialisation is done in the base class constructor 
  // RandomNoiseDigitizer only has to declare it's I/O at the construction time. 
  
  RandomNoiseDigitizer(const std::string& name, ISvcLocator* svcLoc)
      : MultiTransformer(
            name, svcLoc,
            // Input collections for the transformer
            {KeyValues("InputCaloSimHitCollection", {"simplecaloRO"}), KeyValues("HeaderName", {"EventHeader"})},
            // Output collections for the transformer
            {KeyValues("OutputCaloDigiHitCollection", {"RndNoiseCaloDigiHits"})}) {}

  // Initialize
  // initialize() called once, before event processing starts. This is where you do one-time setup work 
  // that shouldn't be repeated for every single event — things like retrieving services, opening files, or setting up histograms, 
  // Retrieving a random number service
  StatusCode initialize() override {

    // Retrieve the UniqueIDGenSvc for generating reproducable random seed
    m_uniqueIDSvc =
        serviceLocator()->service(m_uidSvcName); // Replace with the value from the property for the UniqueIDGenSvc name
    if (!m_uniqueIDSvc) {
      error() << "Unable to locate UniqueIDGenSvc with name: " << m_uidSvcName << endmsg;
      return StatusCode::FAILURE;
    }

    return StatusCode::SUCCESS;
  }

  // Operator: transforms a SimCalorimeterHitCollection into digitized CalorimeterHitCollection
  //excecutd once per event

  std::tuple<edm4hep::CalorimeterHitCollection>
  operator()(const edm4hep::SimCalorimeterHitCollection& InputCaloSimHitCollection,
             const edm4hep::EventHeaderCollection& header) const override {

    // Define a UserDataCollections for digitized output
    auto CaloDigiHits = edm4hep::CalorimeterHitCollection();

    // Initialise and seed random engine for noise generation
    std::mt19937_64 random_engine; //  Mersenne Twister Pseudo Random Number Generator (PRNG) engine
    // Get a unique seed for the random engine based on the event header and the algorithm instance. 
    // So for any given event and any given algorithm instance, this seed — and therefore the entire sequence of 
    // "random" noise added — is always exactly the same across repeated runs
    auto engine_seed = m_uniqueIDSvc->getUniqueID(header, this->name()); 
    random_engine.seed(engine_seed);

    // Create the random distributions for smearing the hit energy
    std::normal_distribution<double> gaussian_noise{
        m_noise_mean.value()*dd4hep::MeV, m_noise_width.value()*dd4hep::MeV}; // Replace with mean and width from properties

    // Loop over the input hits
    for (const auto& hit : InputCaloSimHitCollection) {
      auto digihit = CaloDigiHits.create(); // creates a new, empty hit object inside the output collection for each simulated hit 
      double noise = gaussian_noise(random_engine); // draws one random sample from the Gaussian distribution, using the seeded engine
      digihit.setCellID(hit.getCellID()); 
      //  This is where the actual digitisation happens. New hit energy = Simulated hit energy + random noise (in MeV) 
      digihit.setEnergy(hit.getEnergy() + noise / dd4hep::GeV); 
      digihit.setEnergyError(m_noise_width / dd4hep::GeV); // Set the energy error to the width of the noise distribution (in GeV)
      digihit.setPosition(hit.getPosition()); // Keep the same position as the simulated hit
    }

    // Store the result in the output collection
    //newly built collection is packaged into a std::tuple 
    // std::move () -> prevents unwanted copying of thw hole collection and 
    // since CaliDigiHits is a local variable about to go out of scope; ownership of the collection is transferred to thetuple
    return std::make_tuple(std::move(CaloDigiHits));
  }

private:
  SmartIF<IUniqueIDGenSvc> m_uniqueIDSvc{nullptr};
  // Create a Gaudi::Property for the UniqueIDGenSvc name (specified in the steering file)
  Gaudi::Property<std::string> m_uidSvcName{
      this, "uidSvcName", "UniqueIDGenSvc",
      "The name of the service for generating unique, but reproducable random seeds"};
  // Create two Gaudi::Property members for the noise mean and width
  Gaudi::Property<double> m_noise_mean{this, "NoiseMean", 1e-3, "The mean of the Gaussian noise to be added to the hit energy"};
  Gaudi::Property<double> m_noise_width{this, "NoiseWidth", 1e-4, "The width of the Gaussian noise to be added to the hit energy"}; 
};

DECLARE_COMPONENT(RandomNoiseDigitizer)
