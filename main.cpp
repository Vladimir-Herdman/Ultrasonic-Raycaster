/**
 * @file main.cpp
 * @brief Runs all the code, builds radar off data collected from
 * connected arduino.
 *
 * @author Vladimir Herdman
 * @date 2024-11-29
 * @version 0.5.0
 */
#include <opencv2/opencv.hpp>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include <deque>

// Global Constants section
const int width = 240;  // Go out five rings to measure 50 cm and 10 extra as padding
const int height = 140;  // Go out radius of largest ring (50) and 20 more for top/bottom padding/text
const cv::Size size(width, height);
const int scale = 3;
const cv::Point circle_center(width/2, height - 20);

const cv::Scalar green(0, 180, 0);
const cv::Scalar background(30, 30, 30);

const int fontFace = cv::FONT_HERSHEY_PLAIN;
const double fontScale = 0.5;
const cv::Point angle_display(5, height-5);
const cv::Point distance_display(width/2-20, height-5);

std::deque<std::pair<int, int>> line_deque;
cv::Mat larger_frame = cv::Mat::zeros(size*scale, CV_8UC3);

/**
 * @brief Draws lines and text at an angle
 * 
 * @details This function takes a frame and a length to draw from a starting
 * point at an angle, and then does so to the specified frame.
 * 
 * @param frame The cv::Mat to draw on
 * @param start The starting cv::Point the line will begin at
 * @param angle The degrees from 0-180 to have the line point
 * @param length The length of the line once drawn
 * @param color The color of the line
 * @param with_text To put the angle the line is at past the line when drawn
 */
void drawLineAtAngle(cv::Mat& frame, cv::Point start, int angle, int length, cv::Scalar color, const bool with_text) {
    const double angle_radians = (angle * (M_PI / 180));

    // Section for line
    int end_x = start.x + cos(angle_radians) * length;
    int end_y = start.y - sin(angle_radians) * length;
    cv::line(frame, start, cv::Point(end_x, end_y), color);

    // Section for text
    end_x = start.x + cos(angle_radians) * (length+3);
    end_y = start.y - sin(angle_radians) * (length+3);
    if (angle >= 90) {end_x -= 8;}
    if (with_text){
        cv::putText(frame, std::to_string(angle), cv::Point(end_x, end_y), fontFace, fontScale, green);
    }
}

/**
 * @brief Calculates the point for a screen blip off detected distance
 * 
 * @details This function acts similarly to the above line drawing
 * function, except it doesn't draw on the frame, it just returns the
 * calculated point of where the circle should be.
 * 
 * @param angle The degrees from 0-180 the object was detected
 * @param length The distance at which something was detected
 */
cv::Point calculate_circle_point(const int angle, const int length){
    const double angle_radians = (angle * (M_PI / 180));
    const int end_x = circle_center.x + cos(angle_radians) * length;
    const int end_y = circle_center.y - sin(angle_radians) * length;
    return cv::Point(end_x, end_y);
}

/**
 * @brief Sets up the initial radar used throughout the code
 * 
 * @details Specifically, drawRadar takes a given frame and creates a
 * pre-built template for how the radar will look, this radar then
 * updated throughout the arduino data collection process.
 * 
 * @param frame The cv::Mat to draw on
 */
void drawRadar(cv::Mat& frame){
    // Base frames
    frame = cv::Mat::zeros(size, CV_8UC3);
    frame.setTo(background);

    // Circles
    cv::circle(frame, circle_center, 3, green, -1);
    for (int radius = 1; radius < 6; radius++){
        cv::circle(frame, circle_center, radius*20, green);
    }
    
    // Angle lines
    for (int angle = 1; angle < 6; angle++) {
        drawLineAtAngle(frame, circle_center, angle*30, 104, green, true);
    }
    
    // Bottom info section
    cv::line(frame, cv::Point(0, height-21), cv::Point(width, height-21), green);
    cv::rectangle(frame, cv::Point(0, height-20), cv::Point(width, height), cv::Scalar(15, 15, 15), -1);
    cv::putText(frame, "Degree: ", angle_display, fontFace, 0.8, green);
    cv::putText(frame, "Distance: ", distance_display, fontFace, 0.8, green);

    // Text for circles (ranges)
    for (int radius = 1; radius < 6; radius++){
        cv::putText(frame, std::to_string(radius*10), cv::Point(width/2+radius*20-5, height-17), fontFace, 0.5, green);
    }

    // upscale radar
    cv::resize(frame, larger_frame, larger_frame.size(), 0, 0, cv::INTER_CUBIC);
    cv::imshow("Radar", larger_frame);
    cv::waitKey(1);
}

/**
 * @brief Updates frame with new line and removes old ones.
 * 
 * @details This function draws on a frame the new line based off the
 * degree, and uses the distance to add in a red line for a detected
 * object.  After an amount of lines, the old ones are faded out. A
 * copy of the frame is used so as to simplify the drawing process and
 * start with a blank slate each time.
 * 
 * @param frame The cv::Mat to draw on, not a reference
 * @param degree The angle to draw a line at
 * @param distanceCM The distance at which something was detected
 */
void updateRadar(cv::Mat& frame, const int degree, const int distanceCM){
    drawRadar(frame);
    line_deque.push_front(std::make_pair(degree, distanceCM));

    if (line_deque.size() > 40){line_deque.pop_back();}
    
    // Draw lines and red blips (fade as get farther back)
    int color_change = 0;
    for (auto line : line_deque){
        // line.first: angle | line.second: range detected
        color_change += 5;
        drawLineAtAngle(frame, circle_center, line.first, 100, cv::Scalar(0, 200-color_change, 0), false);
        if (line.second < 50 and line.second > 2){
            cv::circle(frame, calculate_circle_point(line.first, line.second*2), 3, cv::Scalar(0, 8, 255-color_change*1.4), -1);
        }
    }

    // Add data to bottom square area
    cv::putText(frame, std::to_string(degree), cv::Point(angle_display.x+55, angle_display.y), fontFace, 0.8, green);
    if (distanceCM < 50){
        cv::putText(frame, std::to_string(distanceCM)+" cm", cv::Point(distance_display.x+65, distance_display.y), fontFace, 0.8, green);
    } else{
        cv::putText(frame, "Nothing", cv::Point(distance_display.x+65, distance_display.y), fontFace, 0.8, green);
    }

    // Show new radar
    cv::resize(frame, larger_frame, larger_frame.size(), 0, 0, cv::INTER_CUBIC);
    cv::imshow("Radar", larger_frame);
    cv::waitKey(1);
}

int main(){
    // Map for data later used to build raycasting area
    std::map<int, int> arduino_measurements;

    // opencv for displaying radar frame
    cv::Mat radar;
    drawRadar(radar);

    // Set up port reading from arduino program
    const char* port_name = "/dev/tty.usbmodem101";  // from arduino port?
    int serial_port = open(port_name, O_RDWR | O_NOCTTY | O_NDELAY);

    if (serial_port == -1) {
        std::cerr << "Error opening serial port (connect it?): " << strerror(errno) << std::endl;
        return 1;
    }

    struct termios tty;
    if (tcgetattr(serial_port, &tty) != 0) {
        std::cerr << "Error getting terminal attributes: " << strerror(errno) << std::endl;
        close(serial_port);
        return 1;
    }

    cfsetispeed(&tty, B9600);
    cfsetospeed(&tty, B9600);

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag |= CREAD | CLOCAL;

    tcsetattr(serial_port, TCSANOW, &tty);

    // Read in data section, meat of code to update drawings and set up
    // map for later raycast "level"
    char buffer[256];
    std::string data = "";

    while (true){
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = read(serial_port, buffer, sizeof(buffer) - 1);

        if (bytes_read > 0){
            data.append(buffer, bytes_read);
            size_t measurement_delimiter_pos;

            // Parse data for breakpoints in send info
            while ((measurement_delimiter_pos = data.find("|")) != std::string::npos){
                std::string message = data.substr(0, measurement_delimiter_pos);
                data.erase(0, measurement_delimiter_pos + 1);

                // Break up info into specific data points and store
                size_t data_delimiter_pos = message.find(':');

                if (data_delimiter_pos != std::string::npos){
                    const int degree = std::stoi(message.substr(0, data_delimiter_pos));
                    // Here, we take "float" string data and convert to
                    // int to later simplify mapping and drawing
                    const int distanceCM = std::stoi(message.substr(data_delimiter_pos + 1));

                    // Update radar screen and deque
                    updateRadar(radar, degree, distanceCM);

                    // store in measurements if small enough
                    if (distanceCM < 50 && distanceCM > 1 && arduino_measurements.count(degree) == 0){
                        arduino_measurements[degree] = distanceCM;
                    }
                } else {
                    std::cerr << "Invalid message format (breakpoints?): " << message << std::endl;
                }
            }
        }
    }

    // Cleanup and close
    close(serial_port);
    cv::destroyAllWindows();

    return 0;
}