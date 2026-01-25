#include <ctime>
#include <sstream>
#include <string>

using std::string;
class posPacket {
public:
  string pos;
  float speed;
  time_t timeStamp;
  int truckNum;
  float heading;

public:
  posPacket(string pos, float speed, time_t timeStamp, int truckNum, float heading)
      : pos(std::move(pos)), speed(speed), timeStamp(timeStamp),
        truckNum(truckNum), heading(heading) {}
};

string cmd;
posPacket processPacket(const string &command) {
  string pos;
  float speed;
  time_t timeStamp;
  int truckNum;
  float heading;

  std::istringstream stream(command);

  stream >> cmd;
  stream >> pos;
  stream >> speed;
  stream >> timeStamp;
  stream >> truckNum;
  stream >> heading;
  return posPacket(pos, speed, timeStamp, truckNum, heading);
    
}
