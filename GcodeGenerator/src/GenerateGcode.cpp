#include "GenerateGcode.h"

bool GenerateTestGcode(const std::string& Filename)
{
	std::ifstream Input(Filename);
	if (Input.is_open())
	{
		std::ofstream Output("G-code.txt");
		if (Output.is_open())
		{
			Output << "; Generated test G-code\n";
			Output << "F40\n";
			Output << "G91\n";
			std::string Line;
			double X = 0.0;
			double Y = 0.0;

			while (std::getline(Input, Line))
			{
				if (Line.find("CONTOUR") != std::string::npos)
				{
					continue;
				}
				else
				{
					std::istringstream String(Line);
					String >> X >> Y;

					Output << "G0 X" << X / 2 << " Y" << Y / 2 << " Z-10\n";
				}
			}

			Output << "G0 X0 Y0 Z0\n";

			Output.close();
		}
		else
		{
			std::cout << "Error with generate result g-code file";
			Output.close();
			return false;
		}

		Input.close();
	}
	else
	{
		std::cout << "Error with open file: " << Filename;
		Input.close();
		return false;
	}		

	return true;
}

bool SimpleGcodeGenerator::GenerateCode(const std::string& ContourFilename, const std::string& OutputFilename)
{
    std::ifstream Input(ContourFilename);
    std::ofstream Output(OutputFilename);

    if (!Input.is_open() || !Output.is_open()) 
    {
        std::cout << "Error opening files\n";
        return false;
    }
    else
    {
        Output << "; G-code for LEGO printer\n";
        Output << "G21 ; Millimeter units\n";
        Output << "G90 ; Absolute positioning\n";
        Output << "G28 ; Home all axes\n\n";

        std::string Line;
        std::vector<cv::Point> CurrentContour;
        std::string ContourType = "OUTER";
        bool FirstContour = true;
        cv::Point LastPosition(0, 0);

        while (std::getline(Input, Line)) 
        {
            if (Line.empty())
            {
                continue;
            }

            if (Line.find("OUTER_CONTOUR") != std::string::npos ||
                Line.find("INNER_CONTOUR") != std::string::npos) 
            {

                // We process the previous contour
                if (!CurrentContour.empty()) 
                {
                    if (!FirstContour) 
                    {
                        // Raise the nozzle to move
                        Output << "G0 Z" << ZHopHeight << " F" << TravelSpeed << "\n";
                    }

                    // We move to the beginning of the contour
                    cv::Point start_point = CurrentContour[0];
                    Output << "G0 X" << start_point.x / 10.0 << " Y" << start_point.y / 10.0
                        << " F" << TravelSpeed << "\n";

                    // Lower the nozzle and print the outline.
                    Output << "G1 Z0 F" << PrintSpeed << "\n";
                    Output << GenerateGcodeForContour(CurrentContour, ContourType == "OUTER");

                    LastPosition = CurrentContour.back();
                    FirstContour = false;
                }

                // Starting a new circuit
                CurrentContour.clear();
                ContourType = (Line.find("OUTER") != std::string::npos) ? "OUTER" : "INNER";
            }
            else 
            {
                // Reading the coordinates of a point
                std::istringstream iss(Line);
                double X, Y;
                if (iss >> X >> Y) 
                {
                    CurrentContour.push_back(cv::Point(X, Y));
                }
            }
        }

        // We process the last contour
        if (!CurrentContour.empty()) 
        {
            if (!FirstContour) 
            {
                Output << "G0 Z" << ZHopHeight << " F" << TravelSpeed << "\n";
            }

            cv::Point start_point = CurrentContour[0];
            Output << "G0 X" << start_point.x / 10.0 << " Y" << start_point.y / 10.0
                << " F" << TravelSpeed << "\n";
            Output << "G1 Z0 F" << PrintSpeed << "\n";
            Output << GenerateGcodeForContour(CurrentContour, ContourType == "OUTER");
        }

        // We are finishing the program
        Output << "\nG0 Z" << ZHopHeight << " F" << TravelSpeed << "\n";
        Output << "G0 X0 Y0 F" << TravelSpeed << "\n";
        Output << "M2 ; Program end\n";

        return true;
    }    
}

void SimpleGcodeGenerator::SetPrintingParameters(double TravelSpeed, double PrintSpeed, double ZHopHeight)
{
	this->TravelSpeed = TravelSpeed;
	this->PrintSpeed = PrintSpeed;
	this->ZHopHeight = ZHopHeight;
}

std::string SimpleGcodeGenerator::GenerateGcodeForContour(const std::vector<cv::Point>& Contour, bool IsOther)
{
    std::stringstream Gcode;

    // For outer contours - clockwise, for inner ones - counterclockwise
    // This ensures the correct extrusion direction

    if (IsOther)
    {
        // Outer contour - normal order
        for (size_t i = 1; i < Contour.size(); i++) 
        {
            Gcode << "G1 X" << Contour[i].x / 10.0 << " Y" << Contour[i].y / 10.0
                << " F" << PrintSpeed << "\n";
        }
        // Closing the loop
        Gcode << "G1 X" << Contour[0].x / 10.0 << " Y" << Contour[0].y / 10.0
            << " F" << PrintSpeed << "\n";
    }
    else 
    {
        // Inner contour - reverse order for correct filling
        for (int i = Contour.size() - 1; i > 0; i--) 
        {
            Gcode << "G1 X" << Contour[i].x / 10.0 << " Y" << Contour[i].y / 10.0
                << " F" << PrintSpeed << "\n";
        }
        // Closing the loop
        Gcode << "G1 X" << Contour.back().x / 10.0 << " Y" << Contour.back().y / 10.0
            << " F" << PrintSpeed << "\n";
    }

    return Gcode.str();
}
