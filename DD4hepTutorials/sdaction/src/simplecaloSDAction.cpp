
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

#include "DD4hep/Segmentations.h"
#include "DDG4/Factories.h"
#include "DDG4/Geant4GeneratorAction.h"
#include "DDG4/Geant4Mapping.h"
#include "DDG4/Geant4SensDetAction.inl"

#include "G4ThreeVector.hh"
#include "G4TouchableHandle.hh"
#include <cmath>

// #define DEBUG
// The code that runs every time a particle takes a step inside a senstive volume 
namespace dd4hep {
namespace sim {
  class simplecaloSDData {
    // Constructor and destructor
    //
  public:
    simplecaloSDData() = default;
    ~simplecaloSDData() = default;

  public:
    Geant4Sensitive* sensitive{};
  };
} // namespace sim
} // namespace dd4hep

namespace dd4hep {
namespace sim {

  // Function template specialization of Geant4SensitiveAction class.
  // Define actions
  // Called once at the beginning of the simulation. Stores a pointer  to itself in m_userData; 
  // DETAILED_MODE meaning full hit information (position, energy, time, etc.) is recorded, rather than just a summary
  template <>
  void Geant4SensitiveAction<simplecaloSDData>::initialize() {
    m_userData.sensitive = this;
    m_hitCreationMode = HitCreationFlags::DETAILED_MODE;
  }

  // Function template specialization of Geant4SensitiveAction class.
  // Define collections created by this sensitivie action object
  
  // Tells G4 where to store the hits; creates a a hit colletcion named after the readout (simplecaloRO from the XML). 
  // This is how hits end up in the output file under that name.
  template <>
  void Geant4SensitiveAction<simplecaloSDData>::defineCollections() {
    std::string ROname = m_sensitive.readout().name();
    m_collectionID = defineCollection<Geant4Calorimeter::Hit>(ROname);
  }

  // Function template specialization of Geant4SensitiveAction class.
  // Method that accesses the G4Step object at each track step.

  // process(): main function called at every step of the simulation for particles that are in a sensitive volume
  // G4Step contains everything about one simulation step — where the particle is, how much energy it deposited, 
  // what volume it's in, etc.
  template <>
  bool Geant4SensitiveAction<simplecaloSDData>::process(const G4Step* aStep, G4TouchableHistory* /*history*/) {

#ifdef DEBUG // Only compiled in if DEBUG is defined at build time. Prints step-by-step info about the track, 
// position, particle type, energy deposit, material, and volume for each step in the sensitive volume. 
// Useful for understanding what's happening in the simulation at a detailed level.
    std::cout << "-------------------------------" << std::endl;
    std::cout << "--> simplecalo: track info: " << std::endl;
    std::cout << "----> Track #: " << aStep->GetTrack()->GetTrackID() << " "
              << "Step #: " << aStep->GetTrack()->GetCurrentStepNumber() << " "
              << "Volume: " << aStep->GetPreStepPoint()->GetTouchableHandle()->GetVolume()->GetName() << " "
              << std::endl;
    std::cout << "--> simplecalo:: position info(mm): " << std::endl;
    std::cout << "----> x: " << aStep->GetPreStepPoint()->GetPosition().x()
              << " y: " << aStep->GetPreStepPoint()->GetPosition().y()
              << " z: " << aStep->GetPreStepPoint()->GetPosition().z() << std::endl;
    std::cout << "--> simplecalo: particle info: " << std::endl;
    std::cout << "----> Particle " << aStep->GetTrack()->GetParticleDefinition()->GetParticleName() << " "
              << "Dep(MeV) " << aStep->GetTotalEnergyDeposit() << " "
              << "Mat " << aStep->GetPreStepPoint()->GetMaterial()->GetName() << " "
              << "Vol " << aStep->GetPreStepPoint()->GetTouchableHandle()->GetVolume()->GetName() << " " << std::endl;
#endif
  // volumeID(aStep) returns the packed integer ID of the volume where the step occurred, this encodes which layer, 
  // which sublayer, and which cell was hit
  // BitFieldCoder knows how to unpack it: it reserves 5 bits for calolayer (up to 32 layers), 
  // 1 bit for abslayer (0 or 1), and 10 bits for cellid (up to 1024 cells)
    dd4hep::BitFieldCoder decoder("calolayer:5,abslayer:1,cellid:10");
    auto VolID = volumeID(aStep);
#ifdef DEBUG
    auto CaloLayerID = decoder.get(VolID, "calolayer"); // which of the N layers was hit
    auto AbsLayerID = decoder.get(VolID, "abslayer"); // whether the hit was in the absorber (1) or sensitive layer (0)
    auto CellID = decoder.get(VolID, "cellid"); // which pixel (0-99) 
    std::cout << "--> CaloLayerID: " << CaloLayerID << " AbsLayerID " << AbsLayerID << " CellID " << CellID
              << std::endl;
#endif
  // Get the center of the cell's global position; 
  // GetTouchableHandle() : acess to the full geo history; 
  // theTouchable->GetHistory() : retrieves the full geometry navigation history: the chain of nested volumes the particle is 
  // currently inside (e.g. CellVol → SensLayerVol → CaloLayerVol → CaloVol → World).
  // GetTopTransform() : gets the transformation that converts from global coordinates → local coordinates of the current (innermost) volume, i.e. the cell.
  // Inverse().TransformPoint(origin): tranforms local origin (center of teh cell) into global cordinates
  // Cellpos is the position of the center of the cell in global coordinates, which is what we want to store in the hit info
    
    G4TouchableHandle theTouchable = aStep->GetPreStepPoint()->GetTouchableHandle();
    G4ThreeVector origin(0., 0., 0.);
    G4ThreeVector CellPos = theTouchable->GetHistory()->GetTopTransform().Inverse().TransformPoint(origin);
#ifdef DEBUG
    std::cout << "--> Cell global pos(mm) " << CellPos.x() << " " << CellPos.y() << " " << CellPos.z() << std::endl;
#endif

    // Hands-on 5: apply a very short time cut (10 ns) to record the signals
    // and consider the cell border (2 cm) along x and y completely inefficient,
    // i.e. not signal is recorder from that area.
    // Hint: the x,y,z position of the step in the local volume reference frame is
    // G4ThreeVector globalPosition = aStep->GetPreStepPoint()->GetPosition();
    // theTouchable->GetHistory()->GetTopTransform().TransformPoint(globalPosition);
    //

    if (aStep->GetPreStepPoint()->GetGlobalTime() > 10) { // if the global time of the step is greater than 10 ns, skip recording this hit
      return true; // "step processed successfully" from Geant4's pov, 
      // but no hit gets created/updated, since we exit before reaching the hit-creation code 
    } 
    G4ThreeVector globalPosition = aStep->GetPreStepPoint()->GetPosition(); // get the global position of the step (not the cell center, but the actual point where the particle is)
    G4ThreeVector localPosition = theTouchable->GetHistory()->GetTopTransform().TransformPoint(globalPosition); // transform the global position into local coordinates of the cell
    // localPosition is the step's position relative to the center of the cell it's in
    if (std::abs(localPosition.x()) > 30. || std::abs(localPosition.y()) > 30.) { // if the local x or y position is greater than 20 mm (i.e. within 2 cm of the cell border), skip recording this hit
      return true;
    }
    // Hands-on 5: solution
    //
    /*
    if (aStep->GetPreStepPoint()->GetGlobalTime() > 10) {
      return true;
    }
    G4ThreeVector globalPosition = aStep->GetPreStepPoint()->GetPosition();
    G4ThreeVector localPosition =
        theTouchable->GetHistory()->GetTopTransform().TransformPoint(
            globalPosition);
    if (std::abs(localPosition.x()) > 30. || std::abs(localPosition.y()) > 30.) {
      return true;
    }
    // end of Hands-on 5
    */

    // Create the hits and accumulate contributions from multiple steps
    //
    Geant4HitCollection* coll = collection(m_collectionID); // retrieves the collection where all hits for this readout (simplecaloRO) will be stored
    Geant4Calorimeter::Hit* hit = coll->findByKey<Geant4Calorimeter::Hit>(VolID); // the hit; searches the collection: "has this cell already received a hit in this event?"

    if (!hit) { // if the hit does not exist yet, create it
      hit = new Geant4Calorimeter::Hit();
      hit->cellID = VolID; // this should be assigned only once
      // we divide the coordinated by 10 to save them as cm
      Position HitCellPos(CellPos.x() / 10, CellPos.y() / 10, CellPos.z() / 10);
      hit->position = HitCellPos; // this should be assigned only once
      hit->energyDeposit = aStep->GetTotalEnergyDeposit();

      // Add calo hit contributions
      //
      // Crete the first contribution associated to this hit
      Geant4Calorimeter::Hit::Contribution contrib;
      contrib.trackID = aStep->GetTrack()->GetTrackID();
      contrib.pdgID = aStep->GetTrack()->GetParticleDefinition()->GetPDGEncoding();
      contrib.deposit = aStep->GetTotalEnergyDeposit();
      contrib.time = aStep->GetPreStepPoint()->GetGlobalTime();
      contrib.x = HitCellPos.x();
      contrib.y = HitCellPos.y();
      contrib.z = HitCellPos.z();
      hit->truth.emplace_back(contrib);

      coll->add(VolID, hit); // add the hit to the hit collection
    } else {                 // if the hit exists already, increment its fields
      hit->energyDeposit += aStep->GetTotalEnergyDeposit();

      // Add calo hit contributions
      //
      // Add a new contribution associated to this hit
      Geant4Calorimeter::Hit::Contribution contrib;
      contrib.trackID = aStep->GetTrack()->GetTrackID();
      contrib.pdgID = aStep->GetTrack()->GetParticleDefinition()->GetPDGEncoding();
      contrib.deposit = aStep->GetTotalEnergyDeposit();
      contrib.time = aStep->GetPreStepPoint()->GetGlobalTime();
      Position HitCellPos(CellPos.x() / 10, CellPos.y() / 10, CellPos.z() / 10);
      contrib.x = HitCellPos.x();
      contrib.y = HitCellPos.y();
      contrib.z = HitCellPos.z();
      hit->truth.emplace_back(contrib);
    }

    return true;
  } // end of Geant4SensitiveAction::process() method specialization

} // namespace sim
} // namespace dd4hep

//--- Factory declaration
namespace dd4hep {
namespace sim {
  typedef Geant4SensitiveAction<simplecaloSDData> SimpleCaloSDAction;
}
} // namespace dd4hep
DECLARE_GEANT4SENSITIVE(SimpleCaloSDAction)

//**************************************************************************















