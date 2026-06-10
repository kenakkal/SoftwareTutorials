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

// Includers from DD4hep
#include "DDRec/Vector3D.h"
#include <DD4hep/DetFactoryHelper.h>

using namespace dd4hep;

// Build simple calo geometry
//
static Ref_t create_detector(Detector& description, xml_h e, SensitiveDetector sens) {
  std::cout << "--> simplecalo2::create_detector() start" << std::endl;

  // Get info from the xml file
  //
  sens.setType("calorimeter");
  xml_det_t x_det = e;
  std::string det_name = x_det.nameStr();
  std::cout << "--> Going to create " << det_name << ", with ID: " << x_det.id() << std::endl;
  xml_dim_t x_dim = x_det.dimensions();

  const double CaloX = x_dim.x();
  const double CaloY = x_dim.y();
  const double CaloZ = x_dim.z();
  std::cout << "--> calo dimensions from XML description: x " << CaloX / m << " m, y " << CaloY / m << " m, z "
            << CaloZ / m << " m" << std::endl;

  // Retrieve number of layers to populate the calorimeter container with
  //
  auto NumberOfLayers = description.constant<int>("LayersNumber");

  // Info for subdetectors
  //
  xml_det_t x_calo = x_det.child(_Unicode(calo));
  xml_det_t x_calolayer = x_det.child(_Unicode(caloLayer));
  xml_det_t x_abslayer = x_det.child(_Unicode(absLayer));
  xml_det_t x_senslayer = x_det.child(_Unicode(sensLayer));
  xml_det_t x_cell = x_det.child(_Unicode(cell));

  auto iscellsens = x_cell.isSensitive();

  const double CaloLayerX = x_calolayer.x();
  const double CaloLayerY = x_calolayer.y();
  const double CaloLayerZ = x_calolayer.z();

  const double AbsLayerX = x_abslayer.x();
  const double AbsLayerY = x_abslayer.y();
  const double AbsLayerZ = x_abslayer.z();

  const double SensLayerX = x_senslayer.x();
  const double SensLayerY = x_senslayer.y();
  const double SensLayerZ = x_senslayer.z();

  const double CellX = x_cell.x();
  const double CellY = x_cell.y();
  const double CellZ = x_cell.z();

  // Create the geometry
  //

  // Create a container for the calorimeter
  //
  Box Calo(CaloX / 2., CaloY / 2., CaloZ / 2.);
  Volume CaloVol("CaloVol", Calo, description.material(x_calo.attr<std::string>(_U(material))));
  CaloVol.setVisAttributes(description, x_calo.visStr());
  // Creates a box shape (DD4hep boxes are defined by half-lengths, hence the /2.)
  // then wraps it into a Volume with the material and visual attributes read from the XML
  // This is the outermost container for the calorimeter, which will hold all the layers and cells.


  // Create a container for a calorimeter layer
  // (absorber + active elements)
  //
  Box CaloLayer(CaloLayerX / 2., CaloLayerY / 2., CaloLayerZ / 2.);
  Volume CaloLayerVol("CaloLayerVol", CaloLayer, description.material(x_calolayer.attr<std::string>(_U(material))));
  CaloLayerVol.setVisAttributes(description, x_calolayer.visStr());
  // This volume will be replicated multiple times inside the calorimeter container to create the layers. 
  // Each layer will then be filled with an absorber and an active layer.

  // Place twenty calorimeter layers inside the container
  //
  for (std::size_t i = 0; i < static_cast<std::size_t>(NumberOfLayers); i++) {
    PlacedVolume CaloLayerPlaced =
        CaloVol.placeVolume(CaloLayerVol, i, Position(0., 0., -CaloZ / 2. + CaloLayerZ / 2. + i * CaloLayerZ));
    CaloLayerPlaced.addPhysVolID("calolayer", i + 1);
  }
  // The calorimeter layers are placed along the z-axis, starting from the back of the calorimeter (z = -CaloZ/2) 
  // moviing towards the top/front by a half layer thickness (CaloLayerZ/2) to get to the center of the first layer,
  // and stepping forward by the layer thickness (CaloLayerZ) for each subsequent layer

  // Place an absorber layer inside the calorimeter layer
  //
  Box AbsLayer(AbsLayerX / 2., AbsLayerY / 2., AbsLayerZ / 2.);
  Volume AbsLayerVol("AbsLayerVol", AbsLayer, description.material(x_abslayer.attr<std::string>(_U(material))));
  AbsLayerVol.setVisAttributes(description, x_abslayer.visStr());
  PlacedVolume AbsLayerPlaced =
      CaloLayerVol.placeVolume(AbsLayerVol, 1, Position(0., 0., -CaloLayerZ / 2. + AbsLayerZ / 2.));
  AbsLayerPlaced.addPhysVolID("abslayer", 1);
//Brass absorber box is placed at the front (negative z) of each calorimeter layer
// Start at the front face of the layer: -CaloLayerZ/2
// Move right by half the absorber thickness to reach its center: + AbsLayerZ/2
// So the absorber center sits at -CaloLayerZ/2 + AbsLayerZ/2.

  // Place an active layer inside the calorimeter layer
  //
  Box SensLayer(SensLayerX / 2., SensLayerY / 2., SensLayerZ / 2.);
  Volume SensLayerVol("SensLayerVol", SensLayer, description.material(x_senslayer.attr<std::string>(_U(material))));
  SensLayerVol.setVisAttributes(description, x_senslayer.visStr());
  PlacedVolume SensLayerPlaced =
      CaloLayerVol.placeVolume(SensLayerVol, 1, Position(0., 0., CaloLayerZ / 2. - SensLayerZ / 2.));
  SensLayerPlaced.addPhysVolID("abslayer", 0);
  // The active layer is placed at the back of each calorimeter layer, 
  // starting from the back face of the layer (+CaloLayerZ/2) and moving left by half the active layer thickness to 
  // reach its center (-SensLayerZ/2). So within each layer you have absorber followed by sensitive layer 

  // Hands-on 4: Place 100 active cells (pixels) inside the calorimeter sensitive layer
  // and make them sensitive

  Box Cell(CellX / 2., CellY / 2., CellZ / 2.);  
  Volume CellVol("CellVol", Cell, description.material(x_cell.attr<std::string>(_U(material))));
  CellVol.setVisAttributes(description, x_cell.visStr());
  
  //If the cell sensitivity flag is set, hits are recorded at the cell level rather than the whole sensitive layer. 
  // This is what enables you to know which pixel was hit.
  if (iscellsens) 
    CellVol.setSensitiveDetector(sens);
  for (std::size_t i = 0; i < 10; i++) { // Loop over rows; y direction
    for (std::size_t j = 0; j < 10; j++) { // Loop over columns; x direction
      PlacedVolume CellPlaced = SensLayerVol.placeVolume(
          CellVol, i * 10 + j, Position(-SensLayerX / 2. + CellX / 2. + j * CellX, SensLayerY / 2. - CellY / 2. - i * CellY, 0)); 
          // places each cell relative to the center of SensLayerVol
          // For x : start at the left edge of the sensitive layer (-SensLayerX/2), Shift right by half a cell to get to the first cell center (+CellX/2), and then step right by j cells  (+j*CellX)
          // For y : start at the top edge of the sensitive layer (SensLayerY/2), Shift down by half a cell to get to the first cell center (-CellY/2), and then step down by i cells  (-i*CellY)
      CellPlaced.addPhysVolID("cellid", i * 10 + j); // assigns a unique ID from 0 to 99 to each cell, which can be used in the simulation to identify which cell was hit
    }
  }
  
  // Hands-on 4: Solution
  // uncomment the line below to include the solution
  // #include "sc2_solution1.h"

  // Finalize geometry
  //
  DetElement subdet(det_name, x_det.id());
  Volume motherVolume = description.pickMotherVolume(subdet);
  // Place the calo container inside the mother volume
  PlacedVolume CaloPlaced = motherVolume.placeVolume(CaloVol);
  subdet.setPlacement(CaloPlaced);

  std::cout << "--> simplecalo2::create_detector() end" << std::endl;
  return subdet;
}

DECLARE_DETELEMENT(simplecalo2, create_detector)

//**************************************************************************
