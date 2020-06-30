/*
 * Copyright 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "security/facade.h"

#include "grpc/grpc_event_queue.h"
#include "hci/address_with_type.h"
#include "hci/le_address_manager.h"
#include "l2cap/classic/security_policy.h"
#include "os/handler.h"
#include "security/facade.grpc.pb.h"
#include "security/security_manager_listener.h"
#include "security/security_module.h"
#include "security/ui.h"

namespace bluetooth {
namespace security {

class SecurityModuleFacadeService : public SecurityModuleFacade::Service, public ISecurityManagerListener, public UI {
 public:
  SecurityModuleFacadeService(SecurityModule* security_module, ::bluetooth::os::Handler* security_handler)
      : security_module_(security_module), security_handler_(security_handler) {
    security_module_->GetSecurityManager()->RegisterCallbackListener(this, security_handler_);
    security_module_->GetSecurityManager()->SetUserInterfaceHandler(this, security_handler_);
  }

  ::grpc::Status CreateBond(::grpc::ServerContext* context, const facade::BluetoothAddressWithType* request,
                            ::google::protobuf::Empty* response) override {
    hci::Address peer;
    ASSERT(hci::Address::FromString(request->address().address(), peer));
    hci::AddressType peer_type = hci::AddressType::PUBLIC_DEVICE_ADDRESS;
    security_module_->GetSecurityManager()->CreateBond(hci::AddressWithType(peer, peer_type));
    return ::grpc::Status::OK;
  }

  ::grpc::Status CreateBondLe(::grpc::ServerContext* context, const facade::BluetoothAddressWithType* request,
                              ::google::protobuf::Empty* response) override {
    hci::Address peer;
    ASSERT(hci::Address::FromString(request->address().address(), peer));
    hci::AddressType peer_type = static_cast<hci::AddressType>(request->type());
    security_module_->GetSecurityManager()->CreateBondLe(hci::AddressWithType(peer, peer_type));
    return ::grpc::Status::OK;
  }

  ::grpc::Status CancelBond(::grpc::ServerContext* context, const facade::BluetoothAddressWithType* request,
                            ::google::protobuf::Empty* response) override {
    hci::Address peer;
    ASSERT(hci::Address::FromString(request->address().address(), peer));
    hci::AddressType peer_type = hci::AddressType::PUBLIC_DEVICE_ADDRESS;
    security_module_->GetSecurityManager()->CancelBond(hci::AddressWithType(peer, peer_type));
    return ::grpc::Status::OK;
  }

  ::grpc::Status RemoveBond(::grpc::ServerContext* context, const facade::BluetoothAddressWithType* request,
                            ::google::protobuf::Empty* response) override {
    hci::Address peer;
    ASSERT(hci::Address::FromString(request->address().address(), peer));
    hci::AddressType peer_type = hci::AddressType::PUBLIC_DEVICE_ADDRESS;
    security_module_->GetSecurityManager()->RemoveBond(hci::AddressWithType(peer, peer_type));
    return ::grpc::Status::OK;
  }

  ::grpc::Status FetchUiEvents(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                               ::grpc::ServerWriter<UiMsg>* writer) override {
    return ui_events_.RunLoop(context, writer);
  }

  ::grpc::Status SendUiCallback(::grpc::ServerContext* context, const UiCallbackMsg* request,
                                ::google::protobuf::Empty* response) override {
    hci::Address peer;
    ASSERT(hci::Address::FromString(request->address().address().address(), peer));
    hci::AddressType remote_type = static_cast<hci::AddressType>(request->address().type());

    switch (request->message_type()) {
      case UiCallbackType::PASSKEY:
        // TODO: security_module_->GetSecurityManager()->OnPasskeyEntry();
        break;
      case UiCallbackType::YES_NO:
        security_module_->GetSecurityManager()->OnConfirmYesNo(hci::AddressWithType(peer, remote_type),
                                                               request->boolean());
        break;
      case UiCallbackType::PAIRING_PROMPT:
        security_module_->GetSecurityManager()->OnPairingPromptAccepted(
            hci::AddressWithType(peer, remote_type), request->boolean());
        break;
      default:
        LOG_ERROR("Unknown UiCallbackType %d", static_cast<int>(request->message_type()));
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "Unknown UiCallbackType");
    }
    return ::grpc::Status::OK;
  }

  ::grpc::Status FetchBondEvents(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                                 ::grpc::ServerWriter<BondMsg>* writer) override {
    return bond_events_.RunLoop(context, writer);
  }

  ::grpc::Status SetIoCapability(::grpc::ServerContext* context, const IoCapabilityMessage* request,
                                 ::google::protobuf::Empty* response) override {
    security_module_->GetFacadeConfigurationApi()->SetIoCapability(
        static_cast<hci::IoCapability>(request->capability()));
    return ::grpc::Status::OK;
  }

  ::grpc::Status SetLeIoCapability(
      ::grpc::ServerContext* context,
      const LeIoCapabilityMessage* request,
      ::google::protobuf::Empty* response) override {
    security_module_->GetFacadeConfigurationApi()->SetLeIoCapability(
        static_cast<security::IoCapability>(request->capabilities()));
    return ::grpc::Status::OK;
  }

  ::grpc::Status SetAuthenticationRequirements(::grpc::ServerContext* context,
                                               const AuthenticationRequirementsMessage* request,
                                               ::google::protobuf::Empty* response) override {
    security_module_->GetFacadeConfigurationApi()->SetAuthenticationRequirements(
        static_cast<hci::AuthenticationRequirements>(request->requirement()));
    return ::grpc::Status::OK;
  }

  ::grpc::Status SetOobDataPresent(::grpc::ServerContext* context, const OobDataMessage* request,
                                   ::google::protobuf::Empty* response) override {
    security_module_->GetFacadeConfigurationApi()->SetOobData(
        static_cast<hci::OobDataPresent>(request->data_present()));
    return ::grpc::Status::OK;
  }

  ::grpc::Status SetLeAuthReq(
      ::grpc::ServerContext* context, const LeAuthReqMsg* request, ::google::protobuf::Empty* response) override {
    security_module_->GetFacadeConfigurationApi()->SetLeAuthReq(request->auth_req());
    return ::grpc::Status::OK;
  }

  ::grpc::Status SetLeInitiatorAddressPolicy(
      ::grpc::ServerContext* context, const hci::PrivacyPolicy* request, ::google::protobuf::Empty* response) override {
    Address address = Address::kEmpty;
    hci::LeAddressManager::AddressPolicy address_policy =
        static_cast<hci::LeAddressManager::AddressPolicy>(request->address_policy());
    if (address_policy == hci::LeAddressManager::AddressPolicy::USE_STATIC_ADDRESS) {
      ASSERT(Address::FromString(request->address_with_type().address().address(), address));
    }
    hci::AddressWithType address_with_type(address, static_cast<hci::AddressType>(request->address_with_type().type()));
    crypto_toolbox::Octet16 irk = {};
    auto request_irk_length = request->rotation_irk().end() - request->rotation_irk().begin();
    if (request_irk_length == crypto_toolbox::OCTET16_LEN) {
      std::vector<uint8_t> irk_data(request->rotation_irk().begin(), request->rotation_irk().end());
      std::copy_n(irk_data.begin(), crypto_toolbox::OCTET16_LEN, irk.begin());
    } else {
      ASSERT(request_irk_length == 0);
    }
    auto minimum_rotation_time = std::chrono::milliseconds(request->minimum_rotation_time());
    auto maximum_rotation_time = std::chrono::milliseconds(request->maximum_rotation_time());
    security_module_->GetSecurityManager()->SetLeInitiatorAddressPolicy(
        address_policy, address_with_type, irk, minimum_rotation_time, maximum_rotation_time);
    return ::grpc::Status::OK;
  }

  ::grpc::Status FetchEnforceSecurityPolicyEvents(
      ::grpc::ServerContext* context,
      const ::google::protobuf::Empty* request,
      ::grpc::ServerWriter<EnforceSecurityPolicyMsg>* writer) override {
    return enforce_security_policy_events_.RunLoop(context, writer);
  }

  ::grpc::Status EnforceSecurityPolicy(
      ::grpc::ServerContext* context,
      const SecurityPolicyMessage* request,
      ::google::protobuf::Empty* response) override {
    hci::Address peer;
    ASSERT(hci::Address::FromString(request->address().address().address(), peer));
    hci::AddressType peer_type = static_cast<hci::AddressType>(request->address().type());
    hci::AddressWithType peer_with_type(peer, peer_type);
    l2cap::classic::SecurityEnforcementInterface::ResultCallback callback =
        security_handler_->BindOnceOn(this, &SecurityModuleFacadeService::EnforceSecurityPolicyEvent);
    security_module_->GetFacadeConfigurationApi()->EnforceSecurityPolicy(
        peer_with_type, static_cast<l2cap::classic::SecurityPolicy>(request->policy()), std::move(callback));
    return ::grpc::Status::OK;
  }

  void DisplayPairingPrompt(const bluetooth::hci::AddressWithType& peer, std::string name) {
    LOG_INFO("%s", peer.ToString().c_str());
    UiMsg display_yes_no;
    display_yes_no.mutable_peer()->mutable_address()->set_address(peer.ToString());
    display_yes_no.mutable_peer()->set_type(static_cast<facade::BluetoothAddressTypeEnum>(peer.GetAddressType()));
    display_yes_no.set_message_type(UiMsgType::DISPLAY_PAIRING_PROMPT);
    display_yes_no.set_unique_id(unique_id++);
    ui_events_.OnIncomingEvent(display_yes_no);
  }

  virtual void DisplayConfirmValue(const bluetooth::hci::AddressWithType& peer, std::string name,
                                   uint32_t numeric_value) {
    LOG_INFO("%s value = 0x%x", peer.ToString().c_str(), numeric_value);
    UiMsg display_with_value;
    display_with_value.mutable_peer()->mutable_address()->set_address(peer.ToString());
    display_with_value.mutable_peer()->set_type(static_cast<facade::BluetoothAddressTypeEnum>(peer.GetAddressType()));
    display_with_value.set_message_type(UiMsgType::DISPLAY_YES_NO_WITH_VALUE);
    display_with_value.set_numeric_value(numeric_value);
    display_with_value.set_unique_id(unique_id++);
    ui_events_.OnIncomingEvent(display_with_value);
  }

  void DisplayYesNoDialog(const bluetooth::hci::AddressWithType& peer, std::string name) override {
    LOG_INFO("%s", peer.ToString().c_str());
    UiMsg display_yes_no;
    display_yes_no.mutable_peer()->mutable_address()->set_address(peer.ToString());
    display_yes_no.mutable_peer()->set_type(static_cast<facade::BluetoothAddressTypeEnum>(peer.GetAddressType()));
    display_yes_no.set_message_type(UiMsgType::DISPLAY_YES_NO);
    display_yes_no.set_unique_id(unique_id++);
    ui_events_.OnIncomingEvent(display_yes_no);
  }

  void DisplayPasskey(const bluetooth::hci::AddressWithType& peer, std::string name, uint32_t passkey) override {
    LOG_INFO("%s value = 0x%x", peer.ToString().c_str(), passkey);
    UiMsg display_passkey;
    display_passkey.mutable_peer()->mutable_address()->set_address(peer.ToString());
    display_passkey.mutable_peer()->set_type(static_cast<facade::BluetoothAddressTypeEnum>(peer.GetAddressType()));
    display_passkey.set_message_type(UiMsgType::DISPLAY_PASSKEY);
    display_passkey.set_numeric_value(passkey);
    display_passkey.set_unique_id(unique_id++);
    ui_events_.OnIncomingEvent(display_passkey);
  }

  void DisplayEnterPasskeyDialog(const bluetooth::hci::AddressWithType& peer, std::string name) override {
    LOG_INFO("%s", peer.ToString().c_str());
    UiMsg display_passkey_input;
    display_passkey_input.mutable_peer()->mutable_address()->set_address(peer.ToString());
    display_passkey_input.mutable_peer()->set_type(
        static_cast<facade::BluetoothAddressTypeEnum>(peer.GetAddressType()));
    display_passkey_input.set_message_type(UiMsgType::DISPLAY_PASSKEY_ENTRY);
    display_passkey_input.set_unique_id(unique_id++);
    ui_events_.OnIncomingEvent(display_passkey_input);
  }

  void Cancel(const bluetooth::hci::AddressWithType& peer) override {
    LOG_INFO("%s", peer.ToString().c_str());
    UiMsg display_cancel;
    display_cancel.mutable_peer()->mutable_address()->set_address(peer.ToString());
    display_cancel.mutable_peer()->set_type(static_cast<facade::BluetoothAddressTypeEnum>(peer.GetAddressType()));
    display_cancel.set_message_type(UiMsgType::DISPLAY_CANCEL);
    display_cancel.set_unique_id(unique_id++);
    ui_events_.OnIncomingEvent(display_cancel);
  }

  void OnDeviceBonded(hci::AddressWithType peer) override {
    LOG_INFO("%s", peer.ToString().c_str());
    BondMsg bonded;
    bonded.mutable_peer()->mutable_address()->set_address(peer.ToString());
    bonded.mutable_peer()->set_type(static_cast<facade::BluetoothAddressTypeEnum>(peer.GetAddressType()));
    bonded.set_message_type(BondMsgType::DEVICE_BONDED);
    bond_events_.OnIncomingEvent(bonded);
  }

  void OnEncryptionStateChanged(hci::EncryptionChangeView encryption_change_view) override {}

  void OnDeviceUnbonded(hci::AddressWithType peer) override {
    LOG_INFO("%s", peer.ToString().c_str());
    BondMsg unbonded;
    unbonded.mutable_peer()->mutable_address()->set_address(peer.ToString());
    unbonded.mutable_peer()->set_type(static_cast<facade::BluetoothAddressTypeEnum>(peer.GetAddressType()));
    unbonded.set_message_type(BondMsgType::DEVICE_UNBONDED);
    bond_events_.OnIncomingEvent(unbonded);
  }

  void OnDeviceBondFailed(hci::AddressWithType peer) override {
    LOG_INFO("%s", peer.ToString().c_str());
    BondMsg bond_failed;
    bond_failed.mutable_peer()->mutable_address()->set_address(peer.ToString());
    bond_failed.mutable_peer()->set_type(static_cast<facade::BluetoothAddressTypeEnum>(peer.GetAddressType()));
    bond_failed.set_message_type(BondMsgType::DEVICE_BOND_FAILED);
    bond_events_.OnIncomingEvent(bond_failed);
  }

  void EnforceSecurityPolicyEvent(bool result) {
    EnforceSecurityPolicyMsg msg;
    msg.set_result(result);
    enforce_security_policy_events_.OnIncomingEvent(msg);
  }

 private:
  SecurityModule* security_module_;
  ::bluetooth::os::Handler* security_handler_;
  ::bluetooth::grpc::GrpcEventQueue<UiMsg> ui_events_{"UI events"};
  ::bluetooth::grpc::GrpcEventQueue<BondMsg> bond_events_{"Bond events"};
  ::bluetooth::grpc::GrpcEventQueue<EnforceSecurityPolicyMsg> enforce_security_policy_events_{
      "Enforce Security Policy Events"};
  uint32_t unique_id{1};
  std::map<uint32_t, common::OnceCallback<void(bool)>> user_yes_no_callbacks_;
  std::map<uint32_t, common::OnceCallback<void(uint32_t)>> user_passkey_callbacks_;
};

void SecurityModuleFacadeModule::ListDependencies(ModuleList* list) {
  ::bluetooth::grpc::GrpcFacadeModule::ListDependencies(list);
  list->add<SecurityModule>();
}

void SecurityModuleFacadeModule::Start() {
  ::bluetooth::grpc::GrpcFacadeModule::Start();
  service_ = new SecurityModuleFacadeService(GetDependency<SecurityModule>(), GetHandler());
}

void SecurityModuleFacadeModule::Stop() {
  delete service_;
  ::bluetooth::grpc::GrpcFacadeModule::Stop();
}

::grpc::Service* SecurityModuleFacadeModule::GetService() const {
  return service_;
}

const ModuleFactory SecurityModuleFacadeModule::Factory =
    ::bluetooth::ModuleFactory([]() { return new SecurityModuleFacadeModule(); });

}  // namespace security
}  // namespace bluetooth
