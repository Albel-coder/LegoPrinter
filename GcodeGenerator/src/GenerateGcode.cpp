#include "GenerateGcode.h"

bool generateTestGcode(const std::string& inputFilename, const std::string& outputFilename) {
	std::ifstream input(inputFilename);

	if (input.is_open()) {
		std::ofstream output(outputFilename);
		if (output.is_open()) {
			output << "; Generated test G-code\n";
			output << "F40\n";
			output << "G91\n";
			std::string line;
			double x = 0.0;
			double y = 0.0;

			while (std::getline(input, line)) {
				if (line.find("CONTOUR") != std::string::npos) {
					continue;
				}
				else {
					std::istringstream string(line);
					string >> x >> y;

					output << "G0 X" << x / 2 << " Y" << y / 2 << " Z-10\n";
				}
			}

			output << "G0 X0 Y0 Z0\n";

			output.close();
		}
		else {
			//std::cout << "Error with generate result g-code file";
			output.close();
			return false;
		}

		input.close();
	}
	else {
		//std::cout << "Error with open file: " << filename;
		input.close();
		return false;
	}		

	return true;
}

bool SimpleGcodeGenerator::generateCode(const std::string& contourFilename, const std::string& outputFilename) {
    std::ifstream input(contourFilename);
    std::ofstream output(outputFilename);

    if (!input.is_open() || !output.is_open()) {
        //std::cout << "Error opening files\n";
        return false;
    }
    else {
        output << "; G-code for LEGO printer\n";
        output << "G21 ; Millimeter units\n";
        output << "G90 ; Absolute positioning\n";
        output << "G28 ; Home all axes\n\n";

        std::string line;
        std::vector<cv::Point> currentContour;
        std::string contourType = "OUTER";
        bool firstContour = true;
        cv::Point lastPosition(0, 0);

        while (std::getline(input, line)) {
            if (line.empty()) continue;

            if (line.find("OUTER_CONTOUR") != std::string::npos || line.find("INNER_CONTOUR") != std::string::npos) {

                // We process the previous contour
                if (!currentContour.empty()) {
                    if (!firstContour) {
                        // Raise the nozzle to move
                        output << "G0 Z" << zHopHeight << " F" << travelSpeed << "\n";
                    }

                    // We move to the beginning of the contour
                    cv::Point start_point = currentContour[0];
                    output << "G0 X" << start_point.x / 10.0 << " Y" << start_point.y / 10.0
                        << " F" << travelSpeed << "\n";

                    // Lower the nozzle and print the outline.
                    output << "G1 Z0 F" << printSpeed << "\n";
                    output << generateGcodeForContour(currentContour, contourType == "OUTER");

                    lastPosition = currentContour.back();
                    firstContour = false;
                }

                // Starting a new circuit
                currentContour.clear();
                contourType = (line.find("OUTER") != std::string::npos) ? "OUTER" : "INNER";
            }
            else {
                // Reading the coordinates of a point
                std::istringstream iss(line);
                double X, Y;
                if (iss >> X >> Y) {
                    currentContour.push_back(cv::Point(X, Y));
                }
            }
        }

        // We process the last contour
        if (!currentContour.empty()) {
            if (!firstContour) {
                output << "G0 Z" << zHopHeight << " F" << travelSpeed << "\n";
            }

            cv::Point start_point = currentContour[0];
            output << "G0 X" << start_point.x / 10.0 << " Y" << start_point.y / 10.0
                << " F" << travelSpeed << "\n";
            output << "G1 Z0 F" << printSpeed << "\n";
            output << generateGcodeForContour(currentContour, contourType == "OUTER");
        }

        // We are finishing the program
        output << "\nG0 Z" << zHopHeight << " F" << travelSpeed << "\n";
        output << "G0 X0 Y0 F" << travelSpeed << "\n";
        output << "M2 ; Program end\n";

        return true;
    }    
}

void SimpleGcodeGenerator::setPrintingParameters(double travelSpeed, double printSpeed, double zHopHeight) {
	this->travelSpeed = travelSpeed;
	this->printSpeed = printSpeed;
	this->zHopHeight = zHopHeight;
}

std::string SimpleGcodeGenerator::generateGcodeForContour(const std::vector<cv::Point>& contour, bool isOther) {
    std::stringstream gCode;

    // For outer contours - clockwise, for inner ones - counterclockwise
    // This ensures the correct extrusion direction

    if (isOther) {
        // Outer contour - normal order
        for (size_t i = 1; i < contour.size(); i++) {
            gCode << "G1 X" << contour[i].x / 10.0 << " Y" << contour[i].y / 10.0
                << " F" << printSpeed << "\n";
        }
        // Closing the loop
        gCode << "G1 X" << contour[0].x / 10.0 << " Y" << contour[0].y / 10.0
            << " F" << printSpeed << "\n";
    }
    else {
        // Inner contour - reverse order for correct filling
        for (int i = contour.size() - 1; i > 0; i--) {
            gCode << "G1 X" << contour[i].x / 10.0 << " Y" << contour[i].y / 10.0
                << " F" << printSpeed << "\n";
        }
        // Closing the loop
        gCode << "G1 X" << contour.back().x / 10.0 << " Y" << contour.back().y / 10.0
            << " F" << printSpeed << "\n";
    }

    return gCode.str();
}
