// Copyright (c) [2025] [K.Miyawaki]
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#include <iostream>
#include <fstream>
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ros/ros.h>
#include <geometry_msgs/Point.h>
#include <std_msgs/String.h>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

std::vector<std::string> findFiles(const boost::filesystem::path &directory, const boost::regex &pattern);

int main(int argc, char **argv)
{
  ros::init(argc, argv, "mouse_capture");
  ros::NodeHandle pnh("~");

  std::string device_path = "auto";
  double process_rate = 20;
  pnh.getParam("device_path", device_path);
  pnh.getParam("process_rate", process_rate);
  std::map<std::string, std::string> mouse_button_replace;
  {
    XmlRpc::XmlRpcValue val;
    if (pnh.getParam("mouse_button_replace", val))
    {
      for (XmlRpc::XmlRpcValue::const_iterator cit = val.begin(); cit != val.end(); cit++)
      {
        mouse_button_replace.insert(std::make_pair(cit->first, cit->second));
      }
    }
  }
  if (mouse_button_replace.empty() == false)
  {
    for (std::map<std::string, std::string>::const_iterator cit = mouse_button_replace.cbegin(); cit != mouse_button_replace.cend(); cit++)
    {
      ROS_INFO_STREAM(ros::this_node::getName() << ": mouse event string replacer " << cit->first << " -> " << cit->second);
    }
  }

  if (device_path == "auto")
  {
    std::string directory = "/dev/input/by-id";
    std::string pat = ".*event.*mouse";
    std::vector<std::string> paths = findFiles(directory, boost::regex(pat));
    if (paths.empty())
    {
      ROS_ERROR_STREAM(ros::this_node::getName() << ": Can not find mouse in " << directory);
      exit(1);
    }
    device_path = *paths.begin();
    ROS_INFO_STREAM(ros::this_node::getName() << ": Found mouse " << device_path);
  }

  ros::Publisher mouse_move_pub = pnh.advertise<geometry_msgs::Point>("mouse_move", 10);
  ros::Publisher button_pub = pnh.advertise<std_msgs::String>("mouse_button", 10);

  int fd = open(device_path.c_str(), O_RDONLY | O_NONBLOCK);
  if (fd == -1)
  {
    ROS_ERROR_STREAM(ros::this_node::getName() << ": Failed to open " << device_path << ": " << strerror(errno));
    return 1;
  }

  input_event event;

  ros::Rate loop_rate(process_rate);
  while (ros::ok())
  {
    geometry_msgs::Point move;
    while (read(fd, &event, sizeof(event)) == sizeof(event))
    {
      ROS_INFO_STREAM(ros::this_node::getName() << ": event.type " << event.type);
      ROS_INFO_STREAM(ros::this_node::getName() << ": event.code " << event.code);
      if (event.type == EV_REL)
      {
        if (event.code == REL_X)
        {
          move.x += static_cast<double>(event.value);
        }
        else if (event.code == REL_Y)
        {
          move.y += static_cast<double>(event.value);
        }
      }
      else if (event.type == EV_KEY)
      {
        std_msgs::String button_msg;
        if (event.code == BTN_LEFT)
        {
          button_msg.data = (event.value ? "left_down" : "left_up");
        }
        else if (event.code == BTN_RIGHT)
        {
          button_msg.data = (event.value ? "right_down" : "right_up");
        }
        else if (event.code == BTN_MIDDLE)
        {
          button_msg.data = (event.value ? "middle_down" : "middle_up");
        }
        if (button_msg.data.empty() == false)
        {
          std::map<std::string, std::string>::const_iterator cit = mouse_button_replace.find(button_msg.data);
          if (cit != mouse_button_replace.cend())
          {
            button_msg.data = cit->second;
          }
          button_pub.publish(button_msg);
        }
      }
    }
    double d = ::hypot(move.x, move.y);
    if (d > 0)
    {
      mouse_move_pub.publish(move);
    }
    ros::spinOnce();
    loop_rate.sleep();
  }
  close(fd);
  return 0;
}

std::vector<std::string> findFiles(const boost::filesystem::path &directory, const boost::regex &pattern)
{
  std::vector<std::string> result;
  if (!boost::filesystem::exists(directory) || !boost::filesystem::is_directory(directory))
  {
    ROS_ERROR_STREAM(ros::this_node::getName() << ": " << directory << " is not a directory");
    return result;
  }
  boost::filesystem::directory_iterator end_iter;
  for (boost::filesystem::directory_iterator dir_itr(directory); dir_itr != end_iter; ++dir_itr)
  {
    try
    {
      if (regex_match(dir_itr->path().filename().string(), pattern))
      {
        result.push_back(boost::filesystem::absolute(dir_itr->path()).string());
      }
    }
    catch (const std::exception &e)
    {
      ROS_ERROR_STREAM(ros::this_node::getName() << ": " << dir_itr->path() << ": " << e.what());
    }
  }
  return result;
}
