/* RFID Tag Source File
 *
 *
 *
 */

#include "rfidtagmanager.hh"

RFIDTag::RFIDTag(uint32_t tag_id)
{
	// Set pose to (0,0,0) by default
	// TODO

	this->active = true;
	this->tag_id = tag_id;

	return;
}

RFIDTag::RFIDTag(uint32_t tag_id, const gz::math::Pose3d& pose)
{
	// Copy pose from arg
	this->tag_pose = pose;

	this->active = true;
	this->tag_id = tag_id;

	return;
}

ScanResult::ScanResult(uint32_t id, double sig_strength) : tag_id(id), signal_strength(sig_strength)
{
}




