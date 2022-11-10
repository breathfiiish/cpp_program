#ifndef ETH_RELAY_FEATURE_IDS
#define ETH_RELAY_FEATURE_IDS

#include <unordered_set>

typedef std::unordered_set < unsigned int > EthMap;

class EthRelayFeatureIds
{
public:
    // TODO : Add New Feature ID in here.
    unsigned int ETH_RELAY_TEST_FEATURE_ID = 0x12345678;

    EthRelayFeatureIds()
    {
        // TODO: Add this line every time that you add new Feature ID!
        mEthMap.insert(ETH_RELAY_TEST_FEATURE_ID);
    }

    EthRelayFeatureIds(const EthRelayFeatureIds&)=delete;
    EthRelayFeatureIds& operator=(const EthRelayFeatureIds&)=delete;
    uint32_t getEthFeatureSize()
    {
        return mEthMap.size();
    }
    bool isEthFeatureId(const unsigned int id)
    {
        EthMap::iterator it = mEthMap.find(id);
        if (it == mEthMap.end()) {
            return false;
        }
        return true;
    }
    static EthRelayFeatureIds& getInstance()
    {
        return mEthRelayFeatureIds;
    }
private:
    EthMap mEthMap;
    static EthRelayFeatureIds mEthRelayFeatureIds;
};

#endif