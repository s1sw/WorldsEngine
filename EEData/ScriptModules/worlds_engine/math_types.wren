foreign class Vec3 {
    construct new() {
        x = 0
        y = 0
        z = 0
    }

    construct new(x, y, z) {
        setAll(x, y, z)
    }

    foreign getComp(idx)
    foreign setComp(idx, val)

    foreign setAll(x, y, z)

    x { getComp(0) }
    y { getComp(1) }
    z { getComp(2) }

    x=(val) { setComp(0, val) }
    y=(val) { setComp(1, val) }
    z=(val) { setComp(2, val) }
}

