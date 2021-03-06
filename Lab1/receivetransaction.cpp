#include "receivetransaction.h"

#include <QDataStream>
#include <QFile>
#include <QUdpSocket>
#include <QDir>

#include "helpers.h"
#include "message.h"

ReceiveTransaction::ReceiveTransaction( const QHostAddress& addr, quint16 port )
{
    addr_ = addr;
    port_ = port;

    socket_.bind();
    connect( &socket_ ,SIGNAL( readyRead() ), this, SLOT( PendingMessage() ) );
}

void ReceiveTransaction::PendingMessage()
{
    const int for_all_eternity = -1;
    while ( 1 )
    {
        socket_.waitForReadyRead( for_all_eternity );
        ReceiveMessage();
    }
}

void ReceiveTransaction::ReceiveMessage( )
{
    while( socket_.hasPendingDatagrams() )
    {
        QByteArray datagram;
        datagram.resize( socket_.pendingDatagramSize() );
        socket_.readDatagram( datagram.data(), datagram.size() );
        Message msg( datagram );

        if( msg.state == State::Request::REQ_ID )
        {
            RegId( msg );
            return true;
        }
        else if( msg.state == State::Request::SEND_DATA )
        {
            if ( last_seq_for_id_[ msg.id ] < msg.seq )
            {
                last_seq_for_id_[ msg.id ] = msg.seq;
                LoadFile( msg );
            }
            SendMessage( State::Response::RECV_DATA, msg.id, msg.seq  );
        }
        else if( msg.state == State::Request::SEND_FINISH )
        {
            SendFinish( msg );
            DelId( msg )
        }
    }
}

void ReceiveTransaction::SendMessage( quint32 state, quint32 id,  quint32 seq )
{
    QByteArray datagram;
    QDataStream stream( &datagram, QIODevice::ReadWrite );
    stream << Message( state, seq, id, 0 );
    socket_.writeDatagram( datagram, addr_, port_ );
}

void ReceiveTransaction::RegId( Message& msg )
{
    QDataStream in( msg.data );
    QString fName;
    quint64 bytesTotal;
    in >> fName >> bytesTotal;

    while( 1 )
    {
        quint32 ID = rand() % UINT_MAX;
        QMap< quint32, QString >::iterator p = file_for_id_.find( ID );
        if( p == file_for_id_.end( ) )
        {
            file_for_id_.insert( ID, fName );
            SendMessage( State::Response::RESP_ID, ID, msg.seq );
            break;
        }
    }
}

void ReceiveTransaction::DelId( Message& msg )
{
    QMap < quint32, QString >::iterator p = file_for_id_.find( msg.id );
    if( p != file_for_id_.end( ) )
    {
        file_for_id_.erase( p );
    }
    QMap < quint32, quint32 >::iterator p1 = last_seq_for_id_.find( msg.id );
    if( p1 != last_seq_for_id_.end() ) )
    {
        last_seq_for_id_.erase( p1 );
    }

}

void ReceiveTransaction::LoadFile( Message &msg  )
{
    QString path = QDir::currentPath+ QDir::separator() + DOWNLOADS;
    QFile file( path + QDir::separator() + file_for_id_.find( msg.id ) );
    file.open( QIODevice :: Append );
    QDataStream  out( &file );
    out << msg.data;
    file.close();
}

void ReceiveTransaction::SendFinish( Message& msg )
{
    QByteArray datagram;
    QDataStream stream( &datagram, QIODevice::ReadWrite );
    stream << Message( State::Response::RECV_FINISH, msg.seq, msg.id, 0 );
    socket_.writeDatagram( datagram, addr_, port_ );
}
