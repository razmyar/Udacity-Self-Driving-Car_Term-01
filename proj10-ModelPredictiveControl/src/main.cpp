#include <math.h>
#include <uWS/uWS.h>
#include <thread>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "helpers.h"
#include "json.hpp"
#include "MPC.h"

// for convenience
using nlohmann::json;
using std::string;
using std::vector;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }

double deg2rad(double x) { return x * pi() / 180; }

double rad2deg(double x) { return x * 180 / pi(); }

int main() {
    uWS::Hub h;

    // MPC is initialized here!
    MPC mpc;

    h.onMessage([&mpc](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                       uWS::OpCode opCode) {
        // "42" at the start of the message means there's a websocket message event.
        // The 4 signifies a websocket message
        // The 2 signifies a websocket event
        string sdata = string(data).substr(0, length);
        std::cout << sdata << std::endl;
        if (sdata.size() > 2 && sdata[0] == '4' && sdata[1] == '2') {
            string s = hasData(sdata);
            if (s != "") {
                auto j = json::parse(s);
                string event = j[0].get<string>();
                if (event == "telemetry") {
                    // j[1] is the data JSON object
                    vector<double> ptsx = j[1]["ptsx"];
                    vector<double> ptsy = j[1]["ptsy"];
                    double px = j[1]["x"];
                    double py = j[1]["y"];
                    double psi = j[1]["psi"];
                    double v = j[1]["speed"];
                    double delta = j[1]["steering_angle"];
                    double a = j[1]["throttle"];
                    /**
                     * TODO: Calculate steering angle and throttle using MPC.
                     * Both are in between [-1, 1].
                     */

                    vector<double> wps_x;
                    vector<double> wps_y;

                    for (int i = 0; i < ptsx.size(); i++) {
                        double dx = ptsx[i] - px;
                        double dy = ptsy[i] - py;
                        wps_x.push_back(dx * cos(-psi) - dy * sin(-psi));
                        wps_y.push_back(dx * sin(-psi) + dy * cos(-psi));
                    }
                    double *ptrx = &wps_x[0];
                    double *ptry = &wps_y[0];

                    Eigen::Map<Eigen::VectorXd> ptsx_eig(ptrx, 6);
                    Eigen::Map<Eigen::VectorXd> ptsy_eig(ptry, 6);

                    auto coeffs = polyfit(ptsx_eig, ptsy_eig, 3);

                    double cte = polyeval(coeffs, 0);
                    double epsi = -atan(coeffs[1]);

                    VectorXd state(6);
                    const double Lf = 3.67;

                    double dt = 0.1;
                    double x_val = 0.0 + v * dt;
                    double y_val = 0.0;
                    double psi_val = 0.0 - v * delta / Lf * dt;
                    double v_val = v + a * dt;
                    double cte_val = cte + v * sin(epsi) * dt;
                    double epsi_val = epsi + psi_val;

                    state << x_val, y_val, psi_val, v_val, cte_val, epsi_val;

                    auto result = mpc.Solve(state, coeffs);
                    double steer_value = result[0] / (deg2rad(25) * Lf);
                    double throttle_value = result[1];


                    json msgJson;
                    // NOTE: Remember to divide by deg2rad(25) before you send the
                    //   steering value back. Otherwise the values will be in between
                    //   [-deg2rad(25), deg2rad(25] instead of [-1, 1].
                    msgJson["steering_angle"] = steer_value;
                    msgJson["throttle"] = throttle_value;

                    // Display the MPC predicted trajectory
                    vector<double> mpc_x_vals;
                    vector<double> mpc_y_vals;

                    /**
                     * TODO: add (x,y) points to list here, points are in reference to
                     *   the vehicle's coordinate system the points in the simulator are
                     *   connected by a Green line
                     */
                    for (int i = 2; i < result.size() - 1; i += 2) {
                        mpc_x_vals.push_back(result[i]);
                        mpc_y_vals.push_back(result[i + 1]);
                    }

                    msgJson["mpc_x"] = mpc_x_vals;
                    msgJson["mpc_y"] = mpc_y_vals;

                    // Display the waypoints/reference line
                    vector<double> next_x_vals;
                    vector<double> next_y_vals;

                    /**
                     * TODO: add (x,y) points to list here, points are in reference to
                     *   the vehicle's coordinate system the points in the simulator are
                     *   connected by a Yellow line
                     */
                    for (int i = 0; i < 25; i++) {
                        next_x_vals.push_back(2.5 * i);
                        next_y_vals.push_back(polyeval(coeffs, 2.5 * i));
                    }

                    msgJson["next_x"] = next_x_vals;
                    msgJson["next_y"] = next_y_vals;


                    auto msg = "42[\"steer\"," + msgJson.dump() + "]";
                    std::cout << msg << std::endl;
                    // Latency
                    // The purpose is to mimic real driving conditions where
                    //   the car does actuate the commands instantly.
                    //
                    // Feel free to play around with this value but should be to drive
                    //   around the track with 100ms latency.
                    //
                    // NOTE: REMEMBER TO SET THIS TO 100 MILLISECONDS BEFORE SUBMITTING.
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
                }  // end "telemetry" if
            } else {
                // Manual driving
                std::string msg = "42[\"manual\",{}]";
                ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
            }
        }  // end websocket if
    }); // end h.onMessage

    h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
        std::cout << "Connected!!!" << std::endl;
    });

    h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                           char *message, size_t length) {
        ws.close();
        std::cout << "Disconnected" << std::endl;
    });

    int port = 4567;
    if (h.listen(port)) {
        std::cout << "Listening to port " << port << std::endl;
    } else {
        std::cerr << "Failed to listen to port" << std::endl;
        return -1;
    }

    h.run();
}