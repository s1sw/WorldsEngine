using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Game.Util;

public static class EnumerableUtil
{
    public static bool TryFirst<T>(this IEnumerable<T> seq, out T result)
    {
        result = default;
        foreach (var item in seq)
        {
            result = item;
            return true;
        }
        return false;
    }
}
