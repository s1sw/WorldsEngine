using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WorldsEngine.ECS
{
    class SparseStorage
    {
        const int PageSize = 500;
        private List<int[]> _pages = new();

        public SparseStorage()
        {
            // Start off with 1 page
            ResizePages(0);
        }

        private void ResizePages(int requiredIndex)
        {
            for (int i = _pages.Count; i <= requiredIndex; i++)
            {
                _pages.Add(new int[PageSize]);

                for (int j = 0; j < PageSize; j++)
                {
                    _pages[i][j] = -1;
                }
            }
        }

        public int this[Entity entity]
        {
            get
            {
                int pageIndex = (int)(entity.Identifier / PageSize);
                int valIndex = (int)(entity.Identifier % PageSize);

                return _pages[pageIndex][valIndex];
            }

            set
            {
                int pageIndex = (int)(entity.Identifier / PageSize);
                int valIndex = (int)(entity.Identifier % PageSize);

                if (_pages.Count <= pageIndex)
                    ResizePages(pageIndex);

                _pages[pageIndex][valIndex] = value;
            }
        }

        public bool Contains(Entity entity)
        {
            int pageIndex = (int)(entity.Identifier / PageSize);
            int valIndex = (int)(entity.Identifier % PageSize);

            return pageIndex < _pages.Count && _pages[pageIndex][valIndex] != -1;
        }
    }
}
