from bridge_state import TwistCommand


class OutputAdapter:
    def publish_twist(self, twist: TwistCommand) -> None:
        raise NotImplementedError


class DryRunOutput(OutputAdapter):
    def publish_twist(self, twist: TwistCommand) -> None:
        print(
            f"[bridge] cmd_vel source={twist.source} "
            f"linear_x={twist.linear_x:+.3f} angular_z={twist.angular_z:+.3f}",
            flush=True,
        )


class RosOutput(OutputAdapter):
    def __init__(self, node_name: str, topic: str) -> None:
        import rospy
        from geometry_msgs.msg import Twist

        self._twist_type = Twist
        rospy.init_node(node_name, anonymous=False)
        self._publisher = rospy.Publisher(topic, Twist, queue_size=1)
        rospy.loginfo("host_bridge_node publishing Twist to %s", topic)

    def publish_twist(self, twist: TwistCommand) -> None:
        msg = self._twist_type()
        msg.linear.x = twist.linear_x
        msg.angular.z = twist.angular_z
        self._publisher.publish(msg)
