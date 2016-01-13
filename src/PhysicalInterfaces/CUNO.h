/* Copyright 2013-2015 Sathya Laufer
 *
 * Homegear is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Homegear is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Homegear.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */

#ifndef CUNO_H
#define CUNO_H

#include "IBidCoSInterface.h"
#include "AesHandshake.h"

namespace BidCoS
{

class CUNO  : public IBidCoSInterface, public BaseLib::ITimedQueue
{
    public:
		CUNO(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings);
        virtual ~CUNO();
        void startListening();
        void stopListening();
        void sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet);
        void enableUpdateMode();
        void disableUpdateMode();
        virtual bool isOpen() { return _socket->connected(); }

        virtual bool aesSupported() { return true; }
        virtual bool autoResend() { return true; }
        virtual bool needsPeers() { return true; }
        virtual void addPeer(PeerInfo peerInfo);
        virtual void addPeers(std::vector<PeerInfo>& peerInfos);
        virtual void setWakeUp(PeerInfo peerInfo) { addPeer(peerInfo); }
        virtual void setAES(PeerInfo peerInfo, int32_t channel) { addPeer(peerInfo); }
        virtual void removePeer(int32_t address);
    protected:
        class QueueEntry : public BaseLib::ITimedQueueEntry
		{
		public:
			QueueEntry() {}
			QueueEntry(int64_t sendingTime, std::shared_ptr<BidCoSPacket> packet) : ITimedQueueEntry(sendingTime) { this->packet = packet; }
			virtual ~QueueEntry() {}

			std::shared_ptr<BidCoSPacket> packet;
		};

        BaseLib::Obj* _bl = nullptr;
        int64_t _lastAesHandshakeGc = 0;
        std::shared_ptr<AesHandshake> _aesHandshake;
        std::mutex _queueIdsMutex;
        std::map<int32_t, std::set<int64_t>> _queueIds;
        std::mutex _peersMutex;
        std::map<int32_t, PeerInfo> _peers;
        int32_t _myAddress = 0x1C6940;

        std::string _hostname;
        std::string _port;
        std::unique_ptr<BaseLib::SocketOperations> _socket;

        void reconnect();
        void processData(std::vector<uint8_t>& data);
        void send(std::string data);
        std::string readFromDevice();
        void listen();

        void processQueueEntry(int32_t index, int64_t id, std::shared_ptr<BaseLib::ITimedQueueEntry>& entry);
        void queuePacket(std::shared_ptr<BidCoSPacket> packet, int64_t sendingTime = 0);
};

}
#endif // CUNO_H
