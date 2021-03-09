foreign class Random {
    construct new(seed) {
        seedInternal(seed)
    }

    foreign seedInternal(s) 
    foreign float(min, max)
    foreign int(min, max)
    foreign vec3(min, max)
}
