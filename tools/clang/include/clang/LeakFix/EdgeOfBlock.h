class EdgeOfBlock {
public:
    EdgeOfBlock(int pred,int succ,int fk,int uk);
    ~EdgeOfBlock();
    bool isTheEdge(int pred,int succ);
    void setPred(int pred);
    void setSucc(int succ);
    void setFreeKind(int k);
    void setUseKind(int k);
    int getPred();
    int getSucc();
    int getFreeKind();
    int getUseKind();



private:
    int pred_block;
    int succ_block;
    int free_kind;
    int use_kind;
};
