//
// Copyright (C) 2011 OpenSim Ltd.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "inet/common/ResultFilters.h"

#include "inet/applications/base/ApplicationPacket_m.h"
#include "inet/common/FlowTag.h"
#include "inet/common/geometry/common/Coord.h"
#include "inet/common/packet/Packet.h"
#include "inet/common/ResultRecorders.h"
#include "inet/common/Simsignals_m.h"
#include "inet/common/TimeTag_m.h"
#include "inet/mobility/contract/IMobility.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/physicallayer/wireless/common/base/packetlevel/FlatReceptionBase.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"

namespace inet {
namespace utils {
namespace filters {

class INET_API Flow : public cNamedObject
{
  public:
    Flow(const char *name) : cNamedObject(name) { }
};

class INET_API WeightedValue : public cObject
{
  public:
    double weight = NaN;
    cValue value;
};

class INET_API PacketRegionValue : public cObject
{
  public:
    Packet *packet = nullptr;
    b offset = b(-1);
    b length = b(-1);
    cValue value;
};

Register_ResultFilter("dataAge", DataAgeFilter);

void DataAgeFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    if (auto packet = dynamic_cast<Packet *>(object)) {
        for (auto& region : packet->peekData()->getAllTags<CreationTimeTag>()) {
            WeightedHistogramRecorder::cWeight weight(region.getLength().get());
            fire(this, t, t - region.getTag()->getCreationTime(), &weight);
        }
    }
}

Register_ResultFilter("messageAge", MessageAgeFilter);

void MessageAgeFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    if (auto msg = dynamic_cast<cMessage *>(object))
        fire(this, t, t - msg->getCreationTime(), details);
}

Register_ResultFilter("messageTSAge", MessageTsAgeFilter);

void MessageTsAgeFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    if (auto msg = dynamic_cast<cMessage *>(object))
        fire(this, t, t - msg->getTimestamp(), details);
}

Register_ResultFilter("queueingTime", QueueingTimeFilter);

void QueueingTimeFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    if (auto msg = dynamic_cast<cMessage *>(object))
        fire(this, t, t - msg->getArrivalTime(), details);
}

Register_ResultFilter("receptionMinSignalPower", ReceptionMinSignalPowerFilter);

void ReceptionMinSignalPowerFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
#ifdef INET_WITH_PHYSICALLAYERWIRELESSCOMMON
    if (auto reception = dynamic_cast<inet::physicallayer::FlatReceptionBase *>(object)) {
        W minReceptionPower = reception->computeMinPower(reception->getStartTime(), reception->getEndTime());
        fire(this, t, minReceptionPower.get(), details);
    }
#endif // INET_WITH_PHYSICALLAYERWIRELESSCOMMON
}

Register_ResultFilter("appPkSeqNo", ApplicationPacketSequenceNumberFilter);

void ApplicationPacketSequenceNumberFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    if (auto packet = dynamic_cast<Packet *>(object)) {
        if (auto applicationPacket = dynamicPtrCast<const ApplicationPacket>(packet->peekAtFront()))
            fire(this, t, (intval_t)applicationPacket->getSequenceNumber(), details);
    }
}

Register_ResultFilter("mobilityPos", MobilityPosFilter);

void MobilityPosFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    IMobility *module = dynamic_cast<IMobility *>(object);
    if (module) {
        Coord coord = module->getCurrentPosition();
        VoidPtrWrapper wrapper(&coord);
        fire(this, t, &wrapper, details);
    }
}

Register_ResultFilter("xCoord", XCoordFilter);

void XCoordFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    if (auto wrapper = dynamic_cast<VoidPtrWrapper *>(object))
        fire(this, t, ((Coord *)wrapper->getObject())->x, details);
}

Register_ResultFilter("yCoord", YCoordFilter);

void YCoordFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    if (auto wrapper = dynamic_cast<VoidPtrWrapper *>(object))
        fire(this, t, ((Coord *)wrapper->getObject())->y, details);
}

Register_ResultFilter("zCoord", ZCoordFilter);

void ZCoordFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    if (auto wrapper = dynamic_cast<VoidPtrWrapper *>(object))
        fire(this, t, ((Coord *)wrapper->getObject())->z, details);
}

Register_ResultFilter("packetDuration", PacketDurationFilter);

void PacketDurationFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<cPacket *>(object);
    fire(this, t, packet->getDuration(), details);
}

Register_ResultFilter("packetLength", PacketLengthFilter);

void PacketLengthFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<Packet *>(object);
    fire(this, t, packet->getDataLength().get(), details);
}

Register_ResultFilter("flowPacketLength", FlowPacketLengthFilter);

void FlowPacketLengthFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<Packet *>(object);
    auto flow = check_and_cast<Flow *>(details);
    b length = b(0);
    packet->mapAllRegionTags<FlowTag>(b(0), packet->getTotalLength(), [&] (b o, b l, const Ptr<const FlowTag>& flowTag) {
        for (int i = 0; i < flowTag->getNamesArraySize(); i++) {
            if (!strcmp(flowTag->getNames(i), flow->getName()))
                length += l;
        }
    });
    fire(this, t, length.get(), details);
}

Register_ResultFilter("sourceAddr", MessageSourceAddrFilter);

void MessageSourceAddrFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    if (auto *msg = dynamic_cast<Packet *>(object)) {
        Ptr<const L3AddressTagBase> addresses = msg->findTag<L3AddressReq>();
        if (!addresses)
            addresses = msg->findTag<L3AddressInd>();
        if (addresses != nullptr) {
            fire(this, t, addresses->getSrcAddress().str().c_str(), details);
        }
    }
}

Register_ResultFilter("variance", VarianceFilter);

bool VarianceFilter::process(simtime_t& t, double& value, cObject *details)
{
    if (std::isnan(value))
        throw cRuntimeError(this, "collect(): NaN values are not allowed");
    numValues++;
    sumValues += value;
    sumSquaredValues += value * value;
    value = getVariance();
    return true;
}

double VarianceFilter::getVariance() const
{
    // note: no checks for division by zero, we prefer to return Inf or NaN
    if (numValues == 0)
        return NaN;
    else {
        double var = (numValues * sumSquaredValues - sumValues * sumValues) / (numValues * numValues - sumSquaredValues);
        return var < 0 ? 0 : var;
    }
}

Register_ResultFilter("stddev", StddevFilter);

bool StddevFilter::process(simtime_t& t, double& value, cObject *details)
{
    bool result = VarianceFilter::process(t, value, details);
    value = sqrt(value);
    return result;
}

Register_ResultFilter("jitter", JitterFilter);

bool JitterFilter::process(simtime_t& t, double& value, cObject *details)
{
    double jitter = value - last;
    last = value;
    value = jitter;
    return true;
}

Register_ResultFilter("differenceToMean", DifferenceToMeanFilter);

bool DifferenceToMeanFilter::process(simtime_t& t, double& value, cObject *details)
{
    if (std::isnan(value))
        return false;
    count++;
    sum += value;
    value = sum / count - value;
    return true;
}

Register_ResultFilter("utilization", UtilizationFilter);

bool UtilizationFilter::process(simtime_t& t, double& value, cObject *details)
{
    ASSERT(value == 0 || value == 1);
    const simtime_t now = simTime();
    numValues++;
    if (numValues >= numValueLimit) {
        totalValue += value * (now - lastValue).dbl();
        emitUtilization(now, details);
    }
    else if (lastSignal + interval <= now) {
        emitUtilization(lastSignal + interval, details);
        if (emitIntermediateZeros) {
            while (lastSignal + interval <= now)
                emitUtilization(lastSignal + interval, details);
        }
        else {
            if (lastSignal + interval <= now) { // no packets arrived for a long period
                // zero should have been signaled at the beginning of this packet (approximation)
                emitUtilization(now - interval, details);
            }
        }
        totalValue += value * (now - lastValue).dbl();
    }
    else
        totalValue += value * (now - lastValue).dbl();
    lastValue = now;
    return false;
}

void UtilizationFilter::emitUtilization(simtime_t endInterval, cObject *details)
{
    if (totalValue == 0) {
        fire(this, endInterval, 0.0, details);
        lastSignal = endInterval;
    }
    else {
        double utilization = totalValue / (endInterval - lastSignal).dbl();
        fire(this, endInterval, utilization, details);
        lastSignal = endInterval;
        totalValue = 0;
        numValues = 0;
    }
}

void UtilizationFilter::finish(cComponent *component, simsignal_t signal)
{
    const simtime_t now = simTime();
    if (lastSignal < now) {
        cObject *details = nullptr;
        if (lastSignal + interval < now) {
            emitUtilization(lastSignal + interval, details);
            if (emitIntermediateZeros) {
                while (lastSignal + interval < now)
                    emitUtilization(lastSignal + interval, details);
            }
        }
        emitUtilization(now, details);
    }
}

Register_ResultFilter("maxPerGroup", MaxPerGroupFilter);

void MaxPerGroupFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    if (details != identifier) {
        if (identifier != nullptr)
            fire(this, time, max, details);
        max = 0;
        identifier = details;
    }
    auto weightedValue = check_and_cast<WeightedValue *>(object);
    max = std::max(max, weightedValue->value.doubleValue());
    time = t;
}

void MaxPerGroupFilter::finish(cComponent *component, simsignal_t signal)
{
    if (identifier != nullptr)
        fire(this, time, max, nullptr);
}

Register_ResultFilter("weightedMeanPerGroup", WeighedMeanPerGroupFilter);

void WeighedMeanPerGroupFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    if (details != identifier) {
        if (identifier != nullptr)
            fire(this, time, sum / weight, details);
        weight = 0;
        sum = 0;
        identifier = details;
    }
    auto weightedValue = check_and_cast<WeightedValue *>(object);
    sum += weightedValue->weight * weightedValue->value.doubleValue();
    weight += weightedValue->weight;
    time = t;
}

void WeighedMeanPerGroupFilter::finish(cComponent *component, simsignal_t signal)
{
    if (identifier != nullptr)
        fire(this, time, sum / weight, nullptr);
}

Register_ResultFilter("weightedSumPerGroup", WeighedSumPerGroupFilter);

void WeighedSumPerGroupFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    if (details != identifier) {
        if (identifier != nullptr)
            fire(this, time, sum, details);
        sum = 0;
        identifier = details;
    }
    auto weightedValue = check_and_cast<WeightedValue *>(object);
    sum += weightedValue->weight * weightedValue->value.doubleValue();
    time = t;
}

void WeighedSumPerGroupFilter::finish(cComponent *component, simsignal_t signal)
{
    if (identifier != nullptr)
        fire(this, time, sum, nullptr);
}

Register_ResultFilter("dropWeight", DropWeightFilter);

void DropWeightFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto weightedValue = check_and_cast<WeightedValue *>(object);
    fire(this, t, weightedValue->value.doubleValue(), details);
}

Register_ResultFilter("weightTimes", WeightTimesFilter);

void WeightTimesFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto weightedValue = check_and_cast<WeightedValue *>(object);
    if (std::floor(weightedValue->weight) != weightedValue->weight)
        throw cRuntimeError("Only integer values are allowed");
    for (int i = 0; i < weightedValue->weight; i++)
        fire(this, t, weightedValue->value.doubleValue(), details);
}

Register_ResultFilter("groupRegionsPerPacket", GroupRegionsPerPacketFilter);

void GroupRegionsPerPacketFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packetRegionValue = check_and_cast<PacketRegionValue *>(object);
    WeightedValue weightedValue;
    weightedValue.weight = packetRegionValue->length.get();
    weightedValue.value = packetRegionValue->value;
    fire(this, t, &weightedValue, packetRegionValue->packet);
}

Register_ResultFilter("lengthWeightedValuePerRegion", LengthWeightedValuePerRegionFilter);

void LengthWeightedValuePerRegionFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packetRegionValue = check_and_cast<PacketRegionValue *>(object);
    WeightedValue weightedValue;
    weightedValue.weight = packetRegionValue->length.get();
    weightedValue.value = packetRegionValue->value;
    fire(this, t, &weightedValue, details);
}

Register_ResultFilter("demuxFlow", DemuxFlowFilter);

void DemuxFlowFilter::init(cComponent *component, cProperty *attrsProperty)
{
    DemuxFilter::init(component, attrsProperty);
    std::string fullPath = component->getFullPath() + "." + attrsProperty->getIndex() + ".demuxFlow";
    auto value = getEnvir()->getConfig()->getPerObjectConfigValue(fullPath.c_str(), "flowName");
    flowNameMatcher.setPattern(value != nullptr ? value : "*", false, true, true);
}

void DemuxFlowFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    std::set<std::string> flows;
    Packet *packet;
    if (dynamic_cast<Packet *>(object))
        packet = check_and_cast<Packet *>(object);
    else
        packet = check_and_cast<Packet *>(check_and_cast<cPacket *>(object)->getEncapsulatedPacket());
    packet->mapAllRegionTags<FlowTag>(b(0), packet->getTotalLength(), [&] (b o, b l, const Ptr<const FlowTag>& flowTag) {
        for (int i = 0; i < flowTag->getNamesArraySize(); i++) {
            auto flowName = flowTag->getNames(i);
            cMatchableString matchableFlowName(flowName);
            if (flows.find(flowName) == flows.end() && flowNameMatcher.matches(&matchableFlowName)) {
                Flow flow(flowName);
                DemuxFilter::receiveSignal(prev, t, object, &flow);
                flows.insert(flowName);
            }
        }
    });
}

Register_ResultFilter("residenceTimePerRegion", ResidenceTimePerRegionFilter);

void ResidenceTimePerRegionFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<Packet *>(object);
    packet->mapAllRegionTags<ResidenceTimeTag>(b(0), packet->getDataLength(), [&] (b o, b l, const Ptr<const ResidenceTimeTag>& tag) {
        simtime_t startTime = tag->getStartTime();
        simtime_t endTime = tag->getEndTime();
        if (startTime != -1 && endTime != -1) {
            PacketRegionValue packetRegionValue;
            packetRegionValue.packet = packet;
            packetRegionValue.offset = o;
            packetRegionValue.length = l;
            // TODO: no type conversion please
            packetRegionValue.value = cValue((endTime - startTime).dbl());
            fire(this, t, &packetRegionValue, details);
        }
    });
}

Register_ResultFilter("lifeTimePerRegion", LifeTimePerRegionFilter);

void LifeTimePerRegionFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    simtime_t now = simTime();
    auto packet = check_and_cast<Packet *>(object);
    packet->mapAllRegionTags<CreationTimeTag>(b(0), packet->getDataLength(), [&] (b o, b l, const Ptr<const CreationTimeTag>& timeTag) {
        PacketRegionValue packetRegionValue;
        packetRegionValue.packet = packet;
        packetRegionValue.offset = o;
        packetRegionValue.length = l;
        // TODO: no type conversion please
        packetRegionValue.value = cValue((now - timeTag->getCreationTime()).dbl());
        fire(this, t, &packetRegionValue, details);
    });
}

Register_ResultFilter("elapsedTimePerRegion", ElapsedTimePerRegionFilter);

void ElapsedTimePerRegionFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<Packet *>(object);
    packet->mapAllRegionTags<ElapsedTimeTag>(b(0), packet->getDataLength(), [&] (b o, b l, const Ptr<const ElapsedTimeTag>& tag) {
        for (int i = 0; i < (int)tag->getBitTotalTimesArraySize(); i++) {
            PacketRegionValue packetRegionValue;
            packetRegionValue.packet = packet;
            packetRegionValue.offset = o;
            packetRegionValue.length = l;
            // TODO: no type conversion please
            packetRegionValue.value = cValue((simTime() - tag->getBitTotalTimes(i)).dbl());
            fire(this, t, &packetRegionValue, details);
        }
    });
}

Register_ResultFilter("delayingTimePerRegion", DelayingTimePerRegionFilter);

void DelayingTimePerRegionFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<Packet *>(object);
    packet->mapAllRegionTags<DelayingTimeTag>(b(0), packet->getDataLength(), [&] (b o, b l, const Ptr<const DelayingTimeTag>& tag) {
        for (int i = 0; i < (int)tag->getBitTotalTimesArraySize(); i++) {
            PacketRegionValue packetRegionValue;
            packetRegionValue.packet = packet;
            packetRegionValue.offset = o;
            packetRegionValue.length = l;
            // TODO: no type conversion please
            packetRegionValue.value = cValue(tag->getBitTotalTimes(i).dbl());
            fire(this, t, &packetRegionValue, details);
        }
    });
}

Register_ResultFilter("processingTimePerRegion", ProcessingTimePerRegionFilter);

void ProcessingTimePerRegionFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<Packet *>(object);
    packet->mapAllRegionTags<ProcessingTimeTag>(b(0), packet->getDataLength(), [&] (b o, b l, const Ptr<const ProcessingTimeTag>& tag) {
        for (int i = 0; i < (int)tag->getBitTotalTimesArraySize(); i++) {
            PacketRegionValue packetRegionValue;
            packetRegionValue.packet = packet;
            packetRegionValue.offset = o;
            packetRegionValue.length = l;
            // TODO: no type conversion please
            packetRegionValue.value = cValue(tag->getBitTotalTimes(i).dbl());
            fire(this, t, &packetRegionValue, details);
        }
    });
}

Register_ResultFilter("queueingTimePerRegion", QueueingTimePerRegionFilter);

void QueueingTimePerRegionFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<Packet *>(object);
    packet->mapAllRegionTags<QueueingTimeTag>(b(0), packet->getDataLength(), [&] (b o, b l, const Ptr<const QueueingTimeTag>& tag) {
        for (int i = 0; i < (int)tag->getBitTotalTimesArraySize(); i++) {
            PacketRegionValue packetRegionValue;
            packetRegionValue.packet = packet;
            packetRegionValue.offset = o;
            packetRegionValue.length = l;
            // TODO: no type conversion please
            packetRegionValue.value = cValue(tag->getBitTotalTimes(i).dbl());
            fire(this, t, &packetRegionValue, details);
        }
    });
}

Register_ResultFilter("propagationTimePerRegion", PropagationTimePerRegionFilter);

void PropagationTimePerRegionFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<Packet *>(object);
    packet->mapAllRegionTags<PropagationTimeTag>(b(0), packet->getDataLength(), [&] (b o, b l, const Ptr<const PropagationTimeTag>& tag) {
        for (int i = 0; i < (int)tag->getBitTotalTimesArraySize(); i++) {
            PacketRegionValue packetRegionValue;
            packetRegionValue.packet = packet;
            packetRegionValue.offset = o;
            packetRegionValue.length = l;
            // TODO: no type conversion please
            packetRegionValue.value = cValue(tag->getBitTotalTimes(i).dbl());
            fire(this, t, &packetRegionValue, details);
        }
    });
}

Register_ResultFilter("transmissionTimePerRegion", TransmissionTimePerRegionFilter);

void TransmissionTimePerRegionFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<Packet *>(object);
    packet->mapAllRegionTags<TransmissionTimeTag>(b(0), packet->getDataLength(), [&] (b o, b l, const Ptr<const TransmissionTimeTag>& tag) {
        for (int i = 0; i < (int)tag->getBitTotalTimesArraySize(); i++) {
            PacketRegionValue packetRegionValue;
            packetRegionValue.packet = packet;
            packetRegionValue.offset = o;
            packetRegionValue.length = l;
            // TODO: no type conversion please
            packetRegionValue.value = cValue(tag->getBitTotalTimes(i).dbl());
            fire(this, t, &packetRegionValue, details);
        }
    });
}

Register_ResultFilter("packetTransmissionTimePerRegion", PacketTransmissionTimePerRegionFilter);

void PacketTransmissionTimePerRegionFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    auto packet = check_and_cast<Packet *>(object);
    packet->mapAllRegionTags<TransmissionTimeTag>(b(0), packet->getDataLength(), [&] (b o, b l, const Ptr<const TransmissionTimeTag>& tag) {
        for (int i = 0; i < (int)tag->getBitTotalTimesArraySize(); i++) {
            PacketRegionValue packetRegionValue;
            packetRegionValue.packet = packet;
            packetRegionValue.offset = o;
            packetRegionValue.length = l;
            // TODO: no type conversion please
            packetRegionValue.value = cValue(tag->getPacketTotalTimes(i).dbl());
            fire(this, t, &packetRegionValue, details);
        }
    });
}

Register_ResultFilter("throughput", ThroughputFilter);

void ThroughputFilter::emitThroughput(simtime_t endInterval, cObject *details)
{
    if (totalLength == 0) {
        fire(this, endInterval, 0.0, details);
        lastSignal = endInterval;
    }
    else {
        double throughput = totalLength / (endInterval - lastSignal).dbl();
        fire(this, endInterval, throughput, details);
        lastSignal = endInterval;
        totalLength = 0;
        numLengths = 0;
    }
}

void ThroughputFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, intval_t length, cObject *details)
{
    const simtime_t now = simTime();
    numLengths++;
    if (numLengths >= numLengthLimit) {
        totalLength += length;
        emitThroughput(now, details);
    }
    else if (lastSignal + interval <= now) {
        emitThroughput(lastSignal + interval, details);
        if (emitIntermediateZeros) {
            while (lastSignal + interval <= now)
                emitThroughput(lastSignal + interval, details);
        }
        else {
            if (lastSignal + interval <= now) { // no packets arrived for a long period
                // zero should have been signaled at the beginning of this packet (approximation)
                emitThroughput(now - interval, details);
            }
        }
        totalLength += length;
    }
    else
        totalLength += length;
}

void ThroughputFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    if (auto packet = dynamic_cast<Packet *>(object))
        receiveSignal(prev, t, packet->getDataLength().get(), details);
}

void ThroughputFilter::finish(cComponent *component, simsignal_t signalID)
{
    const simtime_t now = simTime();
    if (lastSignal < now) {
        cObject *details = nullptr;
        if (lastSignal + interval < now) {
            emitThroughput(lastSignal + interval, details);
            if (emitIntermediateZeros) {
                while (lastSignal + interval < now)
                    emitThroughput(lastSignal + interval, details);
            }
        }
        emitThroughput(now, details);
    }
}

Register_ResultFilter("liveThroughput", LiveThroughputFilter);

class TimerEvent : public cEvent
{
  protected:
    LiveThroughputFilter *target;

  public:
    TimerEvent(const char *name, LiveThroughputFilter *target) : cEvent(name), target(target) {}
    ~TimerEvent() { target->timerDeleted(); }
    virtual cEvent *dup() const override { copyNotSupported(); return nullptr; }
    virtual cObject *getTargetObject() const override { return target; }
    virtual void execute() override { target->timerExpired(); }
};

void LiveThroughputFilter::init(Context *ctx)
{
    cObjectResultFilter::init(ctx);

    event = new TimerEvent("updateLiveThroughput", this);
    simtime_t now = simTime();
    event->setArrivalTime(now + interval);
    getSimulation()->getFES()->insert(event);
}

LiveThroughputFilter::~LiveThroughputFilter()
{
    if (event) {
        getSimulation()->getFES()->remove(event);
        delete event;
    }
}

void LiveThroughputFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, intval_t length, cObject *details)
{
    totalLength += length;
}

void LiveThroughputFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    if (auto packet = dynamic_cast<cPacket *>(object))
        receiveSignal(prev, t, packet->getByteLength(), details);
}

void LiveThroughputFilter::timerExpired()
{
    simtime_t now = simTime();
    double throughput = totalLength / (now - lastSignal).dbl();
    fire(this, now, throughput, nullptr);
    lastSignal = now;
    totalLength = 0;

    event->setArrivalTime(now + interval);
    getSimulation()->getFES()->insert(event);
}

void LiveThroughputFilter::timerDeleted()
{
    event = nullptr;
}

void LiveThroughputFilter::finish(cComponent *component, simsignal_t signalID)
{
    simtime_t now = simTime();
    if (lastSignal < now) {
        double throughput = totalLength / (now - lastSignal).dbl();
        fire(this, now, throughput, nullptr);
    }
}

Register_ResultFilter("elapsedTime", ElapsedTimeFilter);

ElapsedTimeFilter::ElapsedTimeFilter()
{
    startTime = time(nullptr);
}

double ElapsedTimeFilter::getElapsedTime()
{
    time_t t = time(nullptr);
    return t - startTime;
}

void PacketDropReasonFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    if (check_and_cast<PacketDropDetails *>(details)->getReason() == reason)
        fire(this, t, object, details);
}

// TODO replace these filters with a single filter that supports parameters when it becomes available in omnetpp
#define Register_PacketDropReason_ResultFilter(NAME, CLASS, REASON)    class CLASS : public PacketDropReasonFilter { public: CLASS() { reason = REASON; } }; Register_ResultFilter(NAME, CLASS);
Register_PacketDropReason_ResultFilter("packetDropReasonIsUndefined", UndefinedPacketDropReasonFilter, -1);
Register_PacketDropReason_ResultFilter("packetDropReasonIsAddressResolutionFailed", AddressResolutionFailedPacketDropReasonFilter, ADDRESS_RESOLUTION_FAILED);
Register_PacketDropReason_ResultFilter("packetDropReasonIsForwardingDisabled", ForwardingDisabledPacketDropReasonFilter, FORWARDING_DISABLED);
Register_PacketDropReason_ResultFilter("packetDropReasonIsHopLimitReached", HopLimitReachedPacketDropReasonFilter, HOP_LIMIT_REACHED);
Register_PacketDropReason_ResultFilter("packetDropReasonIsIncorrectlyReceived", IncorrectlyReceivedPacketDropReasonFilter, INCORRECTLY_RECEIVED);
Register_PacketDropReason_ResultFilter("packetDropReasonIsInterfaceDown", InterfaceDownPacketDropReasonFilter, INTERFACE_DOWN);
Register_PacketDropReason_ResultFilter("packetDropReasonIsNoCarrier", NoCarrierPacketDropReasonFilter, NO_CARRIER);
Register_PacketDropReason_ResultFilter("packetDropReasonIsNoInterfaceFound", NoInterfaceFoundPacketDropReasonFilter, NO_INTERFACE_FOUND);
Register_PacketDropReason_ResultFilter("packetDropReasonIsNoRouteFound", NoRouteFoundPacketDropReasonFilter, NO_ROUTE_FOUND);
Register_PacketDropReason_ResultFilter("packetDropReasonIsNotAddressedToUs", NotAddressedToUsPacketDropReasonFilter, NOT_ADDRESSED_TO_US);
Register_PacketDropReason_ResultFilter("packetDropReasonIsQueueOverflow", QueueOverflowPacketDropReasonFilter, QUEUE_OVERFLOW);
Register_PacketDropReason_ResultFilter("packetDropReasonIsRetryLimitReached", RetryLimitReachedPacketDropReasonFilter, RETRY_LIMIT_REACHED);
Register_PacketDropReason_ResultFilter("packetDropReasonIsLifetimeExpired", LifetimeExpiredPacketDropReasonFilter, LIFETIME_EXPIRED);
Register_PacketDropReason_ResultFilter("packetDropReasonIsCongestion", CongestionPacketDropReasonFilter, CONGESTION);
Register_PacketDropReason_ResultFilter("packetDropReasonIsNoProtocolFound", NoProtocolFoundPacketDropReasonFilter, NO_PROTOCOL_FOUND);
Register_PacketDropReason_ResultFilter("packetDropReasonIsNoPortFound", NoPortFoundPacketDropReasonFilter, NO_PORT_FOUND);
Register_PacketDropReason_ResultFilter("packetDropReasonIsDuplicateDetected", DuplicateDetectedPacketDropReasonFilter, DUPLICATE_DETECTED);
Register_PacketDropReason_ResultFilter("packetDropReasonIsOther", OtherPacketDropReasonFilter, OTHER_PACKET_DROP);

Register_ResultFilter("minimumSnir", MinimumSnirFromSnirIndFilter);

void MinimumSnirFromSnirIndFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
#ifdef INET_WITH_PHYSICALLAYERWIRELESSCOMMON
    if (auto pk = dynamic_cast<Packet *>(object)) {
        const auto& tag = pk->findTag<SnirInd>();
        if (tag)
            fire(this, t, tag->getMinimumSnir(), details);
    }
#endif // INET_WITH_PHYSICALLAYERWIRELESSCOMMON
}

Register_ResultFilter("packetErrorRate", PacketErrorRateFromErrorRateIndFilter);

void PacketErrorRateFromErrorRateIndFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
#ifdef INET_WITH_PHYSICALLAYERWIRELESSCOMMON
    if (auto pk = dynamic_cast<Packet *>(object)) {
        const auto& tag = pk->findTag<ErrorRateInd>();
        if (tag)
            fire(this, t, tag->getPacketErrorRate(), details); // TODO isNaN?
    }
#endif // INET_WITH_PHYSICALLAYERWIRELESSCOMMON
}

Register_ResultFilter("bitErrorRate", BitErrorRateFromErrorRateIndFilter);

void BitErrorRateFromErrorRateIndFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
#ifdef INET_WITH_PHYSICALLAYERWIRELESSCOMMON
    if (auto pk = dynamic_cast<Packet *>(object)) {
        const auto& tag = pk->findTag<ErrorRateInd>();
        if (tag)
            fire(this, t, tag->getBitErrorRate(), details); // TODO isNaN?
    }
#endif // INET_WITH_PHYSICALLAYERWIRELESSCOMMON
}

Register_ResultFilter("symbolErrorRate", SymbolErrorRateFromErrorRateIndFilter);

void SymbolErrorRateFromErrorRateIndFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
#ifdef INET_WITH_PHYSICALLAYERWIRELESSCOMMON
    if (auto pk = dynamic_cast<Packet *>(object)) {
        const auto& tag = pk->findTag<ErrorRateInd>();
        if (tag)
            fire(this, t, tag->getSymbolErrorRate(), details); // TODO isNaN?
    }
#endif // INET_WITH_PHYSICALLAYERWIRELESSCOMMON
}

Register_ResultFilter("localSignal", LocalSignalFilter);

void LocalSignalFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, bool b, cObject *details)
{
    fire(this, t, b, details);
}

void LocalSignalFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, intval_t l, cObject *details)
{
    fire(this, t, l, details);
}

void LocalSignalFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, uintval_t l, cObject *details)
{
    fire(this, t, l, details);
}

void LocalSignalFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, double d, cObject *details)
{
    fire(this, t, d, details);
}

void LocalSignalFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, const SimTime& v, cObject *details)
{
    fire(this, t, v, details);
}

void LocalSignalFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, const char *s, cObject *details)
{
    fire(this, t, s, details);
}

void LocalSignalFilter::receiveSignal(cResultFilter *prev, simtime_t_cref t, cObject *object, cObject *details)
{
    fire(this, t, object, details);
}

void LocalSignalFilter::receiveSignal(cComponent *source, simsignal_t signal, bool b, cObject *details)
{
    if (source == component)
        cResultListener::receiveSignal(source, signal, b, details);
}

void LocalSignalFilter::receiveSignal(cComponent *source, simsignal_t signal, intval_t l, cObject *details)
{
    if (source == component)
        cResultListener::receiveSignal(source, signal, l, details);
}

void LocalSignalFilter::receiveSignal(cComponent *source, simsignal_t signal, uintval_t l, cObject *details)
{
    if (source == component)
        cResultListener::receiveSignal(source, signal, l, details);
}

void LocalSignalFilter::receiveSignal(cComponent *source, simsignal_t signal, double d, cObject *details)
{
    if (source == component)
        cResultListener::receiveSignal(source, signal, d, details);
}

void LocalSignalFilter::receiveSignal(cComponent *source, simsignal_t signal, const SimTime& v, cObject *details)
{
    if (source == component)
        cResultListener::receiveSignal(source, signal, v, details);
}

void LocalSignalFilter::receiveSignal(cComponent *source, simsignal_t signal, const char *s, cObject *details)
{
    if (source == component)
        cResultListener::receiveSignal(source, signal, s, details);
}

void LocalSignalFilter::receiveSignal(cComponent *source, simsignal_t signal, cObject *object, cObject *details)
{
    if (source == component)
        cResultListener::receiveSignal(source, signal, object, details);
}

} // namespace filters
} // namespace utils
} // namespace inet

