using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine.Math
{
    public struct Mat3x3
    {
        public float m00, m10, m20;
        public float m01, m11, m21;
        public float m02, m12, m22;

        public Vector3 this[int index] => index switch
        {
            0 => new Vector3(m00, m01, m02),
            1 => new Vector3(m10, m11, m12),
            2 => new Vector3(m20, m21, m22),
            _ => throw new IndexOutOfRangeException()
        };

        public Mat3x3(Vector3 column0, Vector3 column1, Vector3 column2)
        {
            m00 = column0.x;
            m01 = column0.y;
            m02 = column0.z;

            m10 = column1.x;
            m11 = column1.y;
            m12 = column1.z;

            m20 = column2.x;
            m21 = column2.y;
            m22 = column2.z;
        }

        private Quaternion ToQuaternion()
        {
            float tr = m00 + m11 + m22;

            float qw, qx, qy, qz;

            if (tr > 0)
            {
                float S = MathF.Sqrt(tr + 1) * 2f; // S=4*qw 

                qw = 0.25f * S;
                qx = (m12 - m21) / S;
                qy = (m20 - m02) / S;
                qz = (m01 - m10) / S;
            }
            else if ((m00 > m11) && (m00 > m22))
            {
                float S = MathF.Sqrt(1f + m00 - m11 - m22) * 2f; // S=4*qx 

                qw = (m12 - m21) / S;
                qx = 0.25f * S;
                qy = (m10 + m01) / S;
                qz = (m20 + m02) / S;
            }
            else if (m11 > m22)
            {
                float S = MathF.Sqrt(1f + m11 - m00 - m22) * 2f; // S=4*qy

                qw = (m20 - m02) / S;
                qx = (m10 + m01) / S;
                qy = 0.25f * S;
                qz = (m21 + m12) / S;
            }
            else
            {
                float S = MathF.Sqrt(1f + m22 - m00 - m11) * 2f; // S=4*qz

                qw = (m01 - m10) / S;
                qx = (m20 + m02) / S;
                qy = (m21 + m12) / S;
                qz = 0.25f * S;
            }

            return new Quaternion(qw, qx, qy, qz);
        }

        public static explicit operator Quaternion(Mat3x3 mat)
        {
            return mat.ToQuaternion();
        }
    }
}
