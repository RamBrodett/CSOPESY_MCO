struct Frame {
    bool occupied;
    int processId;
    int pageNumber;
};
std::vector<Frame> frameTable;