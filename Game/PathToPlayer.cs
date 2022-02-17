using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using WorldsEngine;
using WorldsEngine.Navigation;
using WorldsEngine.Input;
using WorldsEngine.Math;

namespace Game
{
    [Component]
    public class PathToPlayer : Component, IThinkingComponent
    {
        private NavigationPath _navPath;
        private int _pointIndex = 0;

        public void Think()
        {
            if (Keyboard.KeyPressed(KeyCode.N))
            {
                _navPath = NavigationSystem.FindPath(Entity.Transform.Position, Camera.Main.Position);
                _pointIndex = 0;

                if (!_navPath.Valid)
                {
                    Log.Error("Path wasn't valid!!!");
                }
            }

            if (_navPath == null || !_navPath.Valid) return;

            Vector3 currentTargetPoint = _navPath[_pointIndex];


            Transform t = Entity.Transform;

            if (t.Position.DistanceTo(currentTargetPoint) < 0.5f)
            {
                if (_pointIndex < _navPath.NumPoints - 1)
                    _pointIndex++;
            }
            else
            {
                t.Position += ((currentTargetPoint - t.Position).Normalized * Time.DeltaTime * 5.0f);
                Registry.SetTransform(Entity, t);
            }
        }
    }
}
