/**
 * @file
 *
 * \brief  Declaration of a nodelet simulating a camera
 * \author Corentin Chauvin-Hameau
 * \date   2019
 */

#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "mf_sensors_simulator/CameraOutput.h"
#include "mf_sensors_simulator/MultiPoses.h"
#include "mf_farm_simulator/rviz_visualisation.hpp"
#include "mf_farm_simulator/Algae.h"
#include "reactphysics3d.h"
#include <tf2_ros/transform_listener.h>
#include <tf2/LinearMath/Vector3.h>
#include <tf2/LinearMath/Transform.h>
#include <geometry_msgs/TransformStamped.h>
#include <nodelet/nodelet.h>
#include <ros/ros.h>
#include <csignal>
#include <string>
#include <mutex>
#include <vector>


namespace mfcpp {

typedef std::unique_ptr<rp3d::BoxShape> box_shape_ptr;


/**
 * \brief  Nodelet for a simulated camera
 *
 * For each pixel, the simulated sensor casts a ray. If the ray touches an alga,
 * the sensor will return the hit position, and the corresponding value of
 * the alga disease heatmap.
 *
 * To improve performance, two collision worlds are created:
 * - a big collision world containing all the algae and the field of view (FOV)
 * of the camera.
 * - a small one only for raycasting.
 *
 * A first step is then to test overlap between the camera FOV and the algae, the
 * overlapping algae populates the small collision world. Therefore, raycasting
 * is performed on a smaller amount of algae.
 */
class CameraNodelet: public nodelet::Nodelet {
  public:
    CameraNodelet();
    ~CameraNodelet();

    /**
     * \brief  Function called at beginning of nodelet execution
     */
    virtual void onInit();

  private:
    /**
     * \brief  Callback class for raycasting
     */
    class RaycastCallback: public rp3d::RaycastCallback
    {
      public:
        RaycastCallback(CameraNodelet *parent);
        virtual rp3d::decimal notifyRaycastHit(const rp3d::RaycastInfo& info);

        bool alga_hit_;         ///<  Whether an alga has been hit
        tf2::Vector3 hit_pt_;   ///<  Hit point
        int alga_idx_;          ///<  Index of the hit alga in the ray_bodies_ vector
      private:
        CameraNodelet *parent_;  ///<  Parent CameraNodelet instance
    };

    /**
     * \brief  Callback class for overlap detection
     */
    class OverlapCallback: public rp3d::OverlapCallback
    {
      public:
        OverlapCallback(CameraNodelet *parent);
        virtual void notifyOverlap(rp3d::CollisionBody *body);

      private:
        /// Parent CameraNodelet instance
        CameraNodelet *parent_;
    };


    // Static members
    // Note: the timers need to be static since stopped by the SIGINT callback
    static sig_atomic_t volatile b_sigint_;  ///<  Whether SIGINT signal has been received
    static ros::Timer main_timer_;   ///<  Timer callback for the main function

    // Private members
    ros::NodeHandle nh_;          ///<  Node handler (for topics and services)
    ros::NodeHandle private_nh_;  ///<  Private node handler (for parameters)
    ros::Subscriber algae_sub_;   ///<  Subscriber for the algae of the farm
    ros::ServiceServer ray_multi_serv_;  ///<  Service for raycasting from several camera poses
    ros::Publisher out_pub_;      ///<  Publisher for the camera output
    ros::Publisher rviz_pub_;     ///<  Publisher for Rviz markers
    tf2_ros::Buffer tf_buffer_;
    tf2_ros::TransformListener tf_listener_;

    // FIXME: to remove
    ros::Publisher test_pub_; // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<


    mf_farm_simulator::AlgaeConstPtr last_algae_msg_;  ///<  Last algae message
    bool algae_msg_received_;  ///<  Whether an algae message has been received
    geometry_msgs::TransformStamped fixed_camera_tf_;  ///<  Transform from fixed frame to camera
    geometry_msgs::TransformStamped camera_fixed_tf_;  ///<  Transform from camera to fixed frame
    geometry_msgs::TransformStamped camera_robot_tf_;  ///<  Transform from camera to robot frame
    geometry_msgs::TransformStamped robot_camera_tf_;  ///<  Transform from robot to camera frame
    std::vector<std::vector<std::vector<float>>> heatmaps_;  ///<  Disease heatmatps for all the algae
    std::vector<int> corr_algae_;  ///<  Correspondance between the algae used for raytracing and all the others
    std::mutex coll_mutex_;  ///<  Mutex to control access to collision variables

    /// \name  Collision members
    ///@{
    bool world_init_;   ///<  Whether the collision world has been initialised
    rp3d::CollisionWorld coll_world_;     ///<  World with all algae and camera FOV
    rp3d::CollisionWorld ray_world_;      ///<  World with algae colliding with FOV
    rp3d::WorldSettings world_settings_;  ///<  Collision world settings
    std::vector<rp3d::CollisionBody*> algae_bodies_;  ///<  Collision bodies of the algae
    std::vector<rp3d::CollisionBody*> ray_bodies_;    ///<  Collision bodies for raycasting
    rp3d::CollisionBody* fov_body_;            ///<  Collision body of the camera FOV
    std::vector<box_shape_ptr> algae_shapes_;  ///<  Collision shapes of the algae bodies
    std::vector<box_shape_ptr> ray_shapes_;    ///<  Collision shapes of algae for raycasting
    box_shape_ptr fov_shape_;  ///<  Collision shapes of the FOV body

    RaycastCallback raycast_cb_;  ///<  Callback instance for raycasting
    OverlapCallback overlap_cb_;  ///<  Callback instance for overlap detection
    ///@}


    /// \name  ROS parameters
    ///@{
    float camera_freq_;  ///<  Frequency of the sensor
    std::string fixed_frame_;   ///<  Frame in which the pose is expressed
    std::string robot_frame_;  ///<  Frame of the robot
    std::string camera_frame_;  ///<  Frame of the camera
    std::vector<float> fov_color_;  ///<  Color of the camera field of view Rviz marker
    float focal_length_;    ///<  Focal length of the camera
    float sensor_width_;    ///<  Width of the camera sensor
    float sensor_height_;   ///<  Height of the camera sensor
    float fov_distance_;    ///<  Maximum distance detected by the camera
    int n_pxl_height_;      ///<  Nbr of pixels along sensor height
    int n_pxl_width_;       ///<  Nbr of pixels along sensor width
    ///@}


    /**
     * \brief  Main callback which is called by a timer
     *
     * \param timer_event  Timer event information
     */
    void main_cb(const ros::TimerEvent &timer_event);

    /**
     * \brief  SINGINT (Ctrl+C) callback to stop the nodelet properly
     */
    static void sigint_handler(int s);

    /**
     * \brief  Callback for the algae of the farm
     *
     * \param msg  Pointer to the algae
     */
    void algae_cb(const mf_farm_simulator::AlgaeConstPtr msg);

    /**
     * \brief  Initialise the collision world
     */
    void init_coll_world();

    /**
     * \brief  Updates algae in the collision world
     */
    void update_algae();

    /**
     * \brief  Gets tf transforms
     *
     * \return  Whether a transform has been received
     */
    bool get_tf();

    /**
     * \brief  Fill a collision body for multi-pose FOV overlap
     *
     * The collision body is the smallest AABB containing each FOV of each
     * pose. The corresponding collision shape is also filled.
     *
     * \param [in] poses   Vector of poses
     * \param [out] body   Body to fill
     * \param [out] shape  Collision shape of the body
     */
    void multi_fov_body(const std::vector<geometry_msgs::Pose> &poses,
      rp3d::CollisionBody* body, std::unique_ptr<rp3d::BoxShape> &shape);

    /**
     * \brief  Service callback for raycasting from several camera poses
     *
     * This service raycast one line for each corner of the camera screen. It
     * only returns the hit points position, not the algae disease values.
     */
    bool ray_multi_cb(mf_sensors_simulator::MultiPoses::Request &req,
      mf_sensors_simulator::MultiPoses::Response &res);

    /**
     * \brief  Publishes a Rviz marker for the camera field of view
     */
    void publish_rviz_fov();

    /**
     * \brief  Updates pose of field of view body
     */
    void update_fov_pose();

    /**
     * \brief  Selects algae that are in field of view of the camera
     *
     * The function will update `CameraNodelet::corr_algae_`, `CameraNodelet::ray_world_`,
     * `CameraNodelet::ray_bodies_` and `CameraNodelet::ray_shapes_`.
     *
     * \param body  Collision body to check overlap with algae
     */
    void overlap_fov(rp3d::CollisionBody* body);

    /**
     * \brief  Gets position, dimension and axes of the algae for raycasting
     *
     * \param [out] w_algae   Width of the algae
     * \param [out] h_algae   Height of the algae
     * \param [out] inc_y3    Increment along y3 algae axis
     * \param [out] inc_z3    Increment along z3 algae axis
     * \param [out] tf_algae  Transforms of algae local frames
     */
    void get_ray_algae_carac(
      std::vector<float> &w_algae, std::vector<float> &h_algae,
      std::vector<float> &inc_y3,  std::vector<float> &inc_z3,
      std::vector<geometry_msgs::TransformStamped> &tf_algae
    );

    /**
     * \brief  Gets an aim point from pixel coordinates for raycasting
     *
     * The raycast will be performed between the camera origin and this aim
     * point. The norm of the aim point is the field of view distance.
     *
     * \param pxl_h    Pixel coordinate in height axis
     * \param pxl_w    Pixel coordinate in width axis
     * \param n_pxl_h  Custom total number of pixels of the camera in height (defaulted to global value)
     * \param n_pxl_w  Custom total number of pixels of the camera in width (defaulted to global value)
     * \return  The corresponding aim point
     */
    tf2::Vector3 get_aim_pt(int pxl_h, int pxl_w, int n_pixel_h = -1, int n_pixel_w = -1);

    /**
     * \brief  Converts a tf2 vector to a rp3d vector
     *
     * \param vect  Vector to convert
     */
    inline rp3d::Vector3 tf2_to_rp3d(const tf2::Vector3 vect);

    /**
     * \brief  Converts a pose to a transform
     *
     * \warning  This doesn't take into account frame ids and doesn't fill header
     *
     * \param pose  Pose to convert
     * \return  Converted transform
     */
    inline geometry_msgs::TransformStamped pose_to_tf(const geometry_msgs::Pose &pose);

    /**
     * \brief  Inverse a transform
     *
     * \warning  Doesn't fill the header
     *
     * \param transform  Transform to inverse
     * \return  Inversed transform
     */
    inline geometry_msgs::TransformStamped inverse_tf(
      const geometry_msgs::TransformStamped &transform);

    /**
     * \brief  Applies a transform to a vector
     *
     * \param [in] in_vector  Vector to transform
     * \param [in] transform  Transform to apply
     * return  Transformed vector
     */
    tf2::Vector3 apply_transform(const tf2::Vector3 &in_vector,
      const geometry_msgs::TransformStamped &transform);

    /**
     * \brief  Casts a ray on algae and gets hit point
     *
     * \param [in]  aim_pt    Point towards which casting the ray (in camera frame)
     * \param [out] hit_pt    Hit point (in fixed frame)
     * \param [out] alga_idx  Index of the hit alga in the ray_bodies_ vector
     * \param [in]  origin    Origin of the ray (in camera frame)
     * \return  Whether an alga has been hit
     */
    bool raycast_alga(const tf2::Vector3 &aim_pt, tf2::Vector3 &hit_pt,
      int &alga_idx, const tf2::Vector3 &origin = tf2::Vector3(0, 0, 0));

    /**
     * \brief  Casts a ray on algae and gets hit distance
     *
     * \param [in]  aim_pt    Point towards which casting the ray (in camera frame)
     * \param [out] distance  Distance to the hit alga
     * \param [in]  origin    Origin of the ray (in camera frame)
     * \return  Whether an alga has been hit
     */
    bool raycast_alga(const tf2::Vector3 &aim_pt, float &distance,
      const tf2::Vector3 &origin = tf2::Vector3(0, 0, 0));

    /**
     * \brief  Prepares the ROS output messages
     *
     * \param [out] out_msgs    Camera output message
     * \param [out] ray_marker  Rviz marker for displaying the rays
     * \param [out] pts_marker  Rviz marker for displaying the hit points
     */
    void prepare_out_msgs(
      mf_sensors_simulator::CameraOutput &out_msg,
      visualization_msgs::Marker &ray_marker,
      visualization_msgs::Marker &pts_marker
    );

    /**
     * \brief  Add a point to a point marker
     *
     * \param [out] marker   Marker to fill
     * \param [in]  pt       Point to add to the marker
     * \param [in]  color_r  Red channel of the point color
     * \param [in]  color_g  Green channel of the point color
     * \param [in]  color_b  Blue channel of the point color
     */
    void add_pt_to_marker(visualization_msgs::Marker &marker,
      const tf2::Vector3 &pt, float color_r, float color_g, float color_b);

    /**
     * \brief  Add a point to a point marker
     *
     * \param [out] marker  Marker to fill
     * \param [in]  pt1     First point to add to the marker
     * \param [in]  pt2     Second point to add to the marker
     */
    void add_line_to_marker(visualization_msgs::Marker &marker,
      const tf2::Vector3 &pt1, const tf2::Vector3 &pt2);

    /**
     * \brief  Publishes camera output
     */
    void publish_output();

};


inline rp3d::Vector3 CameraNodelet::tf2_to_rp3d(const tf2::Vector3 vect)
{
  return rp3d::Vector3(vect.getX(), vect.getY(), vect.getZ());
}


inline geometry_msgs::TransformStamped CameraNodelet::pose_to_tf(
  const geometry_msgs::Pose &pose)
{
  geometry_msgs::TransformStamped transf;
  transf.transform.translation.x = pose.position.x;
  transf.transform.translation.y = pose.position.y;
  transf.transform.translation.z = pose.position.z;
  transf.transform.rotation.x = pose.orientation.x;
  transf.transform.rotation.y = pose.orientation.y;
  transf.transform.rotation.z = pose.orientation.z;
  transf.transform.rotation.w = pose.orientation.w;

  return transf;
}


inline geometry_msgs::TransformStamped CameraNodelet::inverse_tf(
  const geometry_msgs::TransformStamped &transform)
{
  tf2::Vector3 trans(transform.transform.translation.x,
                     transform.transform.translation.y,
                     transform.transform.translation.z);
  tf2::Quaternion orient(transform.transform.rotation.x,
                         transform.transform.rotation.y,
                         transform.transform.rotation.z,
                         transform.transform.rotation.w);

  tf2::Transform inverse = tf2::Transform(orient, trans).inverse();
  trans = inverse.getOrigin();
  orient = inverse.getRotation();

  geometry_msgs::TransformStamped ret;
  ret.transform.translation.x = trans.getX();
  ret.transform.translation.y = trans.getY();
  ret.transform.translation.z = trans.getZ();
  ret.transform.rotation.x = orient.getX();
  ret.transform.rotation.y = orient.getY();
  ret.transform.rotation.z = orient.getZ();
  ret.transform.rotation.w = orient.getW();

  return ret;

}


}  // namespace mfcpp

#endif
