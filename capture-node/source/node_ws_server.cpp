// Copyright (C) 2017 Electronic Arts Inc.  All rights reserved.

#include <functional>

#include "node_ws_server.hpp"
#include "capturenode.hpp"
#include "base64.hpp"

#include <boost/algorithm/string.hpp>

// Explicit specialization
template class NodeWSServer<strategy::Secure>;
template class NodeWSServer<strategy::Normal>;
template class WSServer<strategy::Secure>;
template class WSServer<strategy::Normal>;

typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;

// std::string get_password() {
//     return "blabla";
// }

context_ptr on_tls_init(websocketpp::connection_hdl)
{
    std::cout << "on_tls_init" << std::endl;
    context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
    try {
        ctx->set_options(
            boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::no_sslv3 |
            boost::asio::ssl::context::single_dh_use
        );
        //ctx->set_password_callback(std::bind(&get_password));
        ctx->use_certificate_chain_file("server_cert.pem");
        ctx->use_private_key_file("server_key.pem", boost::asio::ssl::context::pem);
        ctx->set_verify_mode(boost::asio::ssl::verify_none); // SECURITY: Disabled certificate verification
    } catch (std::exception& e) {
        std::cout << "on_tls_init failed: " << e.what() << std::endl;
    }
    return ctx;
}

template <>
void WSServer<strategy::Normal>::extra_init(int port)
{
    std::cout << "WEBSOCKET Listening on port " << port << std::endl;
}

template <>
void WSServer<strategy::Secure>::extra_init(int port)
{
    std::cout << "WEBSOCKET Listening on port " << port << " (Secure)" << std::endl;

    server.set_tls_init_handler(&on_tls_init);
}

template <class S>
WSServer<S>::WSServer(int port)
{
    server.init_asio();
	server.set_reuse_addr(true);
    extra_init(port);
    server.clear_access_channels(websocketpp::log::alevel::frame_header | websocketpp::log::alevel::frame_payload); 
    server.set_message_handler(std::bind(&WSServer::on_message, this, std::placeholders::_1, std::placeholders::_2));
    server.listen(port);
    server.start_accept();   
}

template <class S>
WSServer<S>::~WSServer()
{
}

template <class S>
void WSServer<S>::serve_forever_in_thread()
{
	runner = boost::thread([this]() {this->serve_forever(); });
}

template <class S>
void WSServer<S>::serve_forever()
{
    server.run();
}
	
template <class S>
void WSServer<S>::close()
{
    //std::cout << "WEBSOCKET> Shutting down" << std::endl;

    server.stop_listening();
    server.stop();
    runner.join();
}

template <class S>
void WSServer<S>::on_message(websocketpp::connection_hdl hdl, typename S::serverT::message_ptr msg)
{
    //std::cout << "WEBSOCKET> message:" << msg->get_payload() << std::endl;
}

template <class S>
void WSServer<S>::send(websocketpp::connection_hdl hdl, const std::string& s)
{
    server.send(hdl, s, websocketpp::frame::opcode::text);
}

template <class S>
NodeWSServer<S>::NodeWSServer(std::shared_ptr<CaptureNode> pNode, int port)
  : WSServer<S>(port), m_node(pNode)
{    
}

template <class S>
NodeWSServer<S>::~NodeWSServer()
{
}

template <class S>
void NodeWSServer<S>::on_message(websocketpp::connection_hdl hdl, typename S::serverT::message_ptr msg)
{
    //std::cout << "WEBSOCKET> message:" << msg->get_payload() << std::endl;

	std::vector<std::string> paths;
	boost::split(paths, msg->get_payload(), boost::is_any_of("/"), boost::token_compress_on);

    if (paths.size()<3 || msg->get_opcode()!=websocketpp::frame::opcode::text)
        return;

    if (paths[0]=="capture")
    {
        std::string camUniqueId = paths[1];
        std::string action = paths[2];

        auto cam = m_node->cameraById(camUniqueId.c_str());
        if (!cam.get())
            return;

        if (action=="large_preview")
        {
            std::vector<unsigned char> buf;
            if (cam->get_large_preview_image(buf))
            {
        	    WSServer<S>::send(hdl, base64encode(buf));
            }
        }
        else if (action=="preview")
        {
            std::vector<unsigned char> buf;
            bool is_histogram = false;
            if (cam->get_preview_image(buf), &is_histogram)
            {
        	    WSServer<S>::send(hdl, camUniqueId + ";" + base64encode(buf));
            }
        }
    }
}
